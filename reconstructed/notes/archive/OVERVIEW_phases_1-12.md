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
│   ├── prefetch.hpp             # Prefetch (0x160 bytes), PrefetcherNodeState, 30+ methods
│   ├── expression.hpp           # Expression (0x58 bytes), EOperationType/EOperator/ECommandType enums,
│   │                            #   24 create*/copy/str() function declarations
│   └── seqc_parser_context.hpp  # SeqcParserContext stub (currentLineNumber, raiseError, setSyntaxError)
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
│   ├── exception.cpp              # Exception: ctors, dtor, what(), description() (Phase 10.7d)
│   ├── expression.cpp             # Expression: copy ctor, 20+ create* functions, copyExpression,
│   │                              #   seqc_error, str(EOperationType/EOperator/ECommandType)
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
- `AsmExpression` — 0xa8-byte parse tree node. Full layout reconstructed: type (1=reg/2=label/3=int), name (+0x08), str2 (+0x20), command (+0x38), value (+0x3C), children vector (+0x40), optional Label (labelPc +0x5C, labelName +0x60, hasLabel +0x78), optional comment (+0x80, hasComment/noOpt +0x98), isWaveformCmdOverride_ (+0xA0). Factory functions: createValue, createRegister, createName, createArgList, appendArgList. Dtor destroys in reverse: comment→label→children→str2→name

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
- **Waveform Complex (Phase 5, updated 10.5g, CachedParser layout reconstructed in 10.7b-11):** Front/IR split architecture. `WavetableManager<T>` shared template. Wave index assignment: explicit or implicit (auto-fill gaps). Allocation is FIFO-deque based with device granularity alignment. `CachedParser` (0x60 bytes) embedded in both WavetableFront and WavetableIR — layout fully reconstructed in Phase 10.7b-11 (embedded `std::map<vector<uint>, CacheEntry>` + enabled/cacheSize/currentSize/cachePath/indexFilePath; only one ctor `(size_t, path const&)`; methods loadCacheIndex/cleanCache/removeOldFiles identified by address; `CacheEntry` nested-type layout still TBD, see unknowns.md item 78). Phase 10.5e: `getJsonIndex` returns `std::string` via `boost::property_tree` (not `boost::json::value`); `alignWaveformSizes` uses `waveformPageSize` (DC+0x44) not `waveformAlignment`; `assignWaveformAllocationSizes` computes ceil(samples*channels*bitsPerSample/8) rounded to 64 bytes, with `bitsPerSample` at DC+0x50, `waveformGranularity` (DC+0x40) as max cap; `WaveformFront` inlined basic ctor takes `(name, fileType, devConst)`, sets waveIndex=-1, playIndex=1. Phase 10.5g: all 3 allocation lambda bodies fully reconstructed — `allocateWaveforms` Phase 2 lambda (0x2a9c80) performs per-waveform cache-line allocation with conflict checking; `allocateWaveformsForFifo` uses two-pass strategy: $_0 (0x2aa700) allocates irBool0==1 waveforms via `allocateCLAligned`, $_1 (0x2acfb0) handles irBool0==0 with `allocateReloadingCL` fallback. Key: `irBool0` (+0xD9) partitions waveforms into two allocation classes; `irBool1` (+0xDA) = crossesCacheLine flag from MemoryBlock.flags bit 8.
- **AsmOptimize (Phase 6):** 0xA0-byte optimizer with flag-controlled passes. 5 optimization flags (0x01=jumpElim, 0x02=labelCleanup, 0x04=deadCode, 0x08=mergeZeroing, 0x10=regAlloc). Query helpers use `getCmdType()` bitmask (bit 0=reads, bit 1=writes). Register allocator is graph-coloring with greedy allocation; on failure, retries after `splitConstRegisters` (splits long live ranges of constant-loaded registers). `reportUserMessages` extracts MESSAGE/ERROR_MSG pseudo-instructions and routes them through std::function callbacks. `GlobalResources::regNumber` (TLS) provides fresh virtual register numbers for splitting. `OptimizeException` (0x20 bytes) inherits `std::exception` (not `std::runtime_error`).
- **Compiler Pipeline (Phase 7a):** Compiler is a 0x138-byte orchestrator. Pipeline: parse (flex/bison) → toSeqCAst → FrontEndLoweringFacade::lower (virtual dispatch) → linearize → AsmOptimize::optimizePreWaveform → serialize/deserialize round-trip → runPrefetcher (Prefetch + WavetableIR waveform sizing/allocation/placement) → AsmOptimize::optimizePostWaveform → unsyncCervino → output. FrontEndLoweringFacade is a thin static facade — packs args into FrontendLoweringContext, dispatches to SeqCAstNode::lower() virtual. CompilerMessageCollection (0x20 bytes inline) accumulates errors/warnings with duplicate filtering. Debug flags at AWGCompilerConfig+0x90 control AST/tree/assembly printing.
- **Cache (Phase 7b):** 0x28-byte waveform cache with sorted vector of Pointer entries. Two allocation modes: Normal (no alignment doubling) and Aligned (pageSize*2). Gap-finding algorithm (best-fit). PointerState machine: Ready→Playing→LastPlayed→Free. `getScope()` creates a snapshot copy. `unusedCacheLine` sentinel = 0xFFFFFFFF. Error messages use ErrorMessageT indices 0x13–0x16.
- **Prefetch (Phase 7c):** 0x160-byte waveform prefetch manager. Three-pass tree preparation (prepareTree→countBranches→definePlaySize), then placeLoads orchestrates: moveLoadsToFront (hoists loads), optimize (BFS load merging by cache page budget), optimizeSync (placeholder reordering), optimizeCwvf (CWVF config propagation), allocate (cache allocation with branch scoping via Cache::getScope). Key field discoveries: +0xB8=maxBranches_ (not pageSize_), +0xC0=cwvfConfig_ (PlayConfig, not 7 unknowns), +0xF8=lastCwvfNode_, +0x108=globalCwvfValid_. Node+0x18/+0x20 confirmed as load pointer. AWGCompilerConfig+0x18=isHirzel, +0x24=deviceIndex. WaveformIR+0xDA=needsLoad flag.

*Phase 10.7 — TODO Comment Audit (in progress):*
- **Marker count progression:** 174 → 140 → 129 → 120 → 115 → 106 → 101 → 93 → 87 → 84 → 78 → 70
- Method: anti-hallucination — re-disassemble dtor to bound layout; map every TBD
  field to verified binary offsets; merge aliases via forwarding accessors;
  delete pure hallucinations.
- Sub-phase 10.7b-1 (Node, 32 TBD → 0): refCount→type, firstChild→next,
  waveNames→wavesPerDev. _reserved0/_reserved1 at +0x18/+0x20 = decomposed
  weak_ptr<Node> (raw ptr + ctrl block).
- Sub-phase 10.7b-2 (WaveformIR, 11 TBD → 0): proven 0xE0 size via
  __on_zero_shared_weak. Real IR fields: markedForLoad/fixed_/irBool1/elfAlignment_
  in 8 bytes at +0xD8..+0xDF. Caught critical bug: prefetch.cpp:1996 was
  setting wfm->fixed_=false but binary writes +0xD8=1 (different field!).
- Sub-phase 10.7b-3 (WaveformFront, 9 TBD → 0): all 7 alleged TBD fields were
  aliases for existing Waveform/Signal members; args_ was hallucinated.
  Replaced with forwarding accessors (isModified(), funDescrName(), etc.).
- Sub-phase 10.7b-4 (AWGCompilerConfig, 10 TBD → 0): deviceSampleRate is REAL
  (double at +0x08, was unknown_08/0c — verified movsd at 0x1ee69b in
  StaticResources::init). cacheSize/baseGrainSize/channelGrains were really
  devConst_->* fields (NOT config). channelIndex=cacheType, seqCount=deviceIndex.
  appendMode/splitIndex/syncVersion provided as forwarding accessors.
- Sub-phase 10.7b-5 (PrefetcherNodeState + Prefetch, 9 TBD → 0): PNS confirmed
  at 0x40 bytes. 4 alleged fields (lengthReg/counter/playSize/usedCache) were
  aliases for existing PNS slots, given as forwarding accessors. Prefetch
  member `isHirzel_` was alias for `split_` (both at +0xBC). `totalSize` was
  not a PNS field but a function-scoped stack local. `firstTime`, `pageSize_`,
  `npCerv` all hallucinated.
- Sub-phase 10.7b-6 (DeviceConstants aliases, 6 TBD → 0): all 4 extension
  field stubs were aliases for existing 0x90-layout fields. grainSize→
  waveformPageSize (+0x44), maxWaveformLength→waveformGranularity (+0x40),
  maxDioTableEntries→waveformMemorySize (+0x0C), maxWaveIndex→
  (uint32_t)maxSequenceLen (+0x60). Replaced with const accessors;
  static_assert on size restored.
- Sub-phase 10.7b-7 (asm_commands.cpp wrong fields, 8 markers → 0):
  Found that previously-cited addresses were misattributed: real
  genPlayConfig is at 0x2789a0 (not 0x1c3940 = MathCompiler::log),
  asmPlay at 0x278b40 (not 0x1c4a60). genPlayConfig field offsets
  corrected: [wf+0xc8] = signal.channels_ (uint16_t), [wf+0xb0/+0xb8]
  = signal.markerBits_ vec begin/end. Critical removal: asmPlay does
  NOT call PlayConfig::encodeCwvf; that call (0x1dc500) has exactly one
  caller in the entire binary — 0x1d8075 in Prefetch::placeSingleCommand
  — and the asmPlay reconstruction's `currentWvf->playWord =
  config.encodeCwvf(...)` block was a hallucination, removed.
  Stale Immediate-variant TODO (line 286) also cleared.
- Sub-phase 10.7b-8 (AWGAssemblerImpl, 6 TBD → 0): total size 0x170
  (verified: mov edi,0x170 at 0x285050 in AWGAssembler ctor). Major
  hallucinations removed:
    * field_70_ (void*) — real field is int32_t currentLine_,
      read by errorMessage as the leading int of an emitted Message
      and zeroed at assembleString+0x16c.
    * Message struct: only ONE int field (named code_), not two.
      errorMessage writes currentLine_, parserMessage writes the level
      arg, into the same slot.
    * opcodes_begin_/end_ + sourceLines_begin_/end_ — never existed
      in the binary; pipeline.cpp updated to use std::vector .data()/
      .size()/.push_back() / .empty().
    * remaining_fields_[0x80] — IS the embedded AsmParserContext at
      +0xF0 (size exactly 0x80 = 0x170 - 0xF0). Verified by
      errorMessage's `add rbx,0xf0; call setSyntaxError` writing byte
      at parser ctx +0x03 (= AWG +0xf3 = hadSyntaxError_).
    * Duplicate fields removed (flags_, initialized_, pad0_,
      lineNumber_) — these were each first-few-bytes of the embedded
      parserCtx_ incorrectly listed at AWGAssemblerImpl scope.
  Cannot host-static_assert sizeof: binary is libc++ (string=24),
  build host is libstdc++ (string=32). Layout proven from disasm.
- Sub-phase 10.7b-9 (ErrorMessageT placeholders, 3 markers → 0):
  Resolved InvalidRegister=0, ValueOverflow=5, UnsupportedOp=11 by
  decoding `mov esi,...` immediate before each ErrorMessages::format
  call site (prf 0x271446, wvfs 0x2719b1, Cervino wvfs/wvft/wwvfq
  0x27c2c8/0x27c3a8/0x27c488). Cross-referenced against the BSS map
  initializer at 0xd5de0 to confirm the format strings.
  CRITICAL: discovery of an off-by-one shift across the entire
  SeqC-compiler portion of the existing enum (entries 1..12) — the
  binary uses keys 0..11 for the same strings the existing enum
  numbered 1..12. Three new aliases coexist correctly with the
  existing names (C++ enums permit duplicate values), but a full
  renumbering of CmdWithoutRegister..InvalidOpcode is needed in a
  future pass.
- Sub-phase 10.7b-10 (Signal convenience methods, 4 markers → 0):
  `Signal::granularity/maxLength/minLength/bitsPerSample` confirmed
  absent from the binary — entirely fabricated. The few callers were
  actually reading DeviceConstants fields (waveformPageSize @+0x44,
  waveformGranularity @+0x40, bitsPerSample @+0x50) or
  Waveform::seqRegWidth @+0x70 (the JSON "minLengthSamples" field).
  Verified at three sites: Prefetch::definePlaySize 0x1ca918 and
  0x1cab18 (devConst from `this->devConst_` and from `wf->deviceConstants`
  respectively) and WavetableFront::getMemorySize 0x29ae31 (from
  `wf->deviceConstants`). Removed fakes from signal.hpp; replaced
  call sites in prefetch_prepare.cpp and wavetable_front.cpp with
  direct field reads. Preserved cmova → max() semantics. Full build
  clean (0 errors).
- Sub-phase 10.7b-11 (Minor header TBDs, 8 markers → 0):
    * Cache::Pointer::pageCount removed — no symbol, no consumer,
      Pointer is exactly 0x24 bytes (verified via 0x282b27..282b49).
    * AsmList::Asm::lineNumber resolved as forwarding accessor for
      wavetableFront — same 4 bytes at +0x88 used dual-purpose: as
      wavetableFront for normal asm commands, as lineNumber for
      MESSAGE/ERROR_MSG (verified at 0x280c02 `mov eax,[r13+0x88]`
      reading line number for callbacks).
    * AsmExpression: pad_58/labelPc were aliases for a single
      int32_t at +0x58. Renamed canonical field to `labelIndex`,
      provided labelPc()/lineNumber()/noOpt()/labelType() as
      forwarding accessor methods (replacing reference data members
      that were silently breaking layout). Verified at 0x285732
      and 0x287e43. Total size 0xA8 (alloc'd as 0xC0 inside
      __shared_ptr_emplace<AsmExpression> @ 0x2877db).
    * Resources VarSubType (0=default, 1=boolean) and EDirection
      (0=read, 1=write/strict) populated with verified values.
    * CachedParser layout (0x60 bytes) reconstructed from
      ctor 0x2afa70 + dtor 0x29aac0: embedded
      std::map<vector<uint>, CacheEntry> at +0x00..+0x30,
      enabled_ bool at +0x18, cacheSize_ at +0x20, currentSize_
      at +0x28, cachePath_ (boost::path = std::string) at +0x30,
      indexFilePath_ at +0x48 = cachePath_/"index". The fabricated
      default ctor was removed; only (size_t, path const&) exists
      in the binary. Five methods identified by address (ctor, dtor,
      loadCacheIndex, cleanCache, removeOldFiles).
    * ZIAWGCompilerException reconciled — actually inherits from
      zhinst::Exception (which derives from std::bad_exception +
      boost::exception via MI), total size 0x60. Removed fabrications:
      no `(int code, std::string)` ctor, no `code_` field, no
      `code()` method. Two real ctors at 0x2e72f0 (default — hardcodes
      "compiler exception", magic string visible as
      `0x6e6f697470656378` = "xception") and 0x2e7360 (string by
      value). For source-level compatibility the reconstructed
      class still inherits std::exception with a local message_
      field; full zhinst::Exception base class reconstruction is
      deferred to a future sub-phase. Full build clean (0 errors).
- Sub-phase 10.7c (Documentation, marker total 70): added
  `notes/libcpp_abi.md` documenting libc++ vs libstdc++ ABI
  differences (string=24 vs 32, function=0x30 vs 0x20, __tree=0x30
  both, basic_string tagged-SSO layout, shared_ptr/weak_ptr control
  block layouts, `__on_zero_shared_weak` deallocation-size pattern
  for proving class sizes from disasm, why we cannot host-static_assert
  sizeof for string-bearing structs, the value.cpp placement-new
  warning's ABI-mismatch root cause). Updated `notes/unknowns.md`
  with items 78 (CacheEntry layout), 79 (zhinst::Exception base
  class — MI with std::bad_exception + boost::exception, ctors and
  methods enumerated), 80 (GenericErrorDescription<T> template).
   This document directly enables Phase 10.8 work on src/ marker
   hotspots, where many TODOs are libc++ weak_ptr ABI workarounds
   that can now be properly resolved.

- Sub-phase 10.8a (prefetch.cpp focus, marker total 49):
  resolved all 18 markers in `src/prefetch.cpp` plus 3 cascading
  TODOs in `src/prefetch_print.cpp`. Total 70 → 49.
  Key changes:
  * **Node loadRef refactor**: replaced `Node* loadRef_ptr` +
    `void* loadRef_ctrl` pair (binary's libc++ weak_ptr decomposition
    at +0x18..+0x20) with a single `std::weak_ptr<Node> loadRef`.
    Layout preserved (16 bytes either ABI). Updated all ~12 call
    sites across prefetch.cpp, prefetch_print.cpp, prefetch_splitplay.cpp,
    prefetch_placesingle.cpp, and node.cpp to use clean `loadRef.lock()`
    and `loadRef = sharedPtr` semantics. Eliminated all
    "non-owning shared_ptr with `[](Node*){}` deleter" workarounds.
  * **Switch-case Play/Load mislabel correction**: verified three
    type-switch jump tables via .rodata dump
    (`Prefetch::optimizeCwvf` at 0x95aed8, `Prefetch::allocate` at
    0x95af54, `Prefetch::prepareTree` at 0x95ae98, `Prefetch::print`
    at 0x95ad98). Found systematic Load↔Play mislabeling in three
    functions (the prior reconstruction had labels swapped because
    NodeType::Load=1 maps to jump-table idx 0, while NodeType::Play=2
    maps to idx 1 — easy to misread when annotating). Corrected:
    - `Prefetch::allocate`: case `NodeType::Play` (line 1492) →
      `NodeType::Load`; uncommented previously-disabled "duplicate
      case 2" block as the real `case NodeType::Play` (it is not a
      duplicate; it is the actual idx-1 case body at 0x1d130d that
      calls `Cache::play(loadCachePtr, pointerState)`).
    - `Prefetch::prepareTree` (`prefetch_prepare.cpp`): swapped
      labels at the case bodies for 0x1c8a6f (Load) and 0x1c8d13
      (Play, where `linkLoad(current)` is called).
    - `Prefetch::print` (`prefetch_print.cpp`): swapped labels at
      0x1c5f1a (Load — prints "load asmID..." or "load <wavename>")
      and 0x1c636f (Play).
    - `Prefetch::optimizeCwvf` case at 0x1d02a8 corrected to be
      `case NodeType::Play: case NodeType::Table:` — both types
      share the body (Play via jump-table idx 0, Table via
      `cmp ecx,0x200; jne` fall-through). The inner `cmp ecx,0x2`
      (preserved type field) selects the Play-only CWVF eligibility
      branch.
  * **Goto-label "missing labels" resolved**: the two `goto recheckCwvf`
    / `goto recheckLoopCwvf` markers refer to `je 0x1cfe5b` /
    `je 0x1d0523` instructions in the binary's *non-Play else-branch*
    that re-enter the Play CWVF check. Verified these are
    structurally unreachable in source (we are inside an `else` arm
    where `type != Play`, yet the binary tests for Play again) —
    treated as compiler artifacts (likely from inlined-then-DCE'd
    template/switch). Replaced TODO with explanatory comment.
  * **Bitmask 0x114 verified**: the APPROXIMATE flag was unwarranted.
    Binary at 0x1d079c contains literal `mov edx,0x114; bt edx,ecx`
    encoding the test `scanType ∈ {Play(2), Branch(4), Loop(8)}`
    as a bitset.
  * **shared_ptr `get_deleter` workaround removed**: at 0x1d0395 the
    binary does a standard 16-byte shared_ptr copy of `curShared`
    (load ptr+ctrl from rbp slots, lock-inc ctrl). No custom deleter
    is involved — the prior reconstruction misread the
    pre-call ABI shuffling as needing get_deleter.
  * **WaveformIR::name typo**: the local `optional<string> waveName`
    was sloppy — `WaveformIR` inherits from `Waveform` whose `name`
    field at +0 is a plain `std::string`. The optional wrapper
    happens at the destination slot (`wavesPerDev[i]` is
    `optional<string>`); the source-level temporary should be
    `std::string`. Fixed.
  * Added jump-table address citations in cleaned-up case comments
    so future readers can verify the dispatch quickly without
    re-running .rodata dumps.

  Build status after 10.8a: full -fsyntax-only clean, 0 errors,
  only the pre-existing 8-line value.cpp placement-new warning
  (libcpp_abi.md documents this as a libc++/libstdc++ ABI
  mismatch artifact). Saved at `/tmp/build_errors_108a/full.err`.

- Sub-phase 10.8b (prefetch_placesingle.cpp + prefetch_splitplay.cpp,
  marker total 49 → 39):

  * **placesingle (6 → 0)**:
    - Resolved `currentAsmId_` field name: the binary writes
      `asmCommands_->wavetableFrontIndex_` (AsmCommands+0x50) from
      `placeholder->wavetableFront` (Asm+0x88). The reconstructed source
      cannot express this directly because `findPlaceholder` was
      declared returning `shared_ptr<Node>` rather than `AsmList::iterator`;
      kept as a precise comment with a follow-up structural note.
    - Resolved `loadNode` initialization: `np->loadRef.lock()` (the same
      back-reference pattern resolved in 10.8a).
    - Cleared stale TODO on `WaveformIR+0xC8`: confirmed `signal.channels_`
      is correct via `Waveform::signal` at +0x80 + `Signal::channels_`
      at +0x48 → +0xC8. Loop iterates over channels, not pages
      (the source's `numPages` variable name is misleading but accurate
      in semantics — it's a per-channel ssl emission).
    - Lifted `int totalSize` to outer-scope of the case-1 block so the
      three indexed branches that compute it (lines ~493/529/581) can
      share with the consumer at play_common_prf (lines 636/644).
      This matches the binary's stack-slot `-0x140(%rbp)` flow.
    - **Side discovery (documented, not acted on)**: The monolithic
      `case 1:` body in placeSingleCommand actually merges genuine
      NodeType::Load code (0x1d79f8..0x1d7a4b + 0x1d8281+) with
      NodeType::Play code (0x1d7d49+) under a single label. Verified
      via jump table at 0x95af74 (entries 0..7, indexed by type-1).
      A future structural pass should split into proper case Load
      and case Play. File-header comment added.

  * **splitplay (4 → 0)**:
    - Cache::Pointer field corrections: at 0x1dd4de and 0x1dd524 the
      binary reads `cachePtr->numRepeats_` (+0x0C), not `pageSize` or
      `pageSize_` as the source claimed. At 0x1de0cf reads
      `cachePtr->hash_` (+0x08), not `position_` (+0). Both fixed.
    - APPROXIMATE comment removed: the "decrements by 1" comment on the
      counter-register addi was a misread; `mov esi,0x1` at 0x1de274
      proves the immediate is +1 (which the source code already had
      correct).

  Anti-hallucination methodology continues to pay dividends: the prior
  reconstructor's source attribution had several systematic errors that
  only show up via direct disassembly verification of struct field offsets
  and jump-table targets, never from "looks plausible" reading.

  Build status after 10.8b: full -fsyntax-only clean, 0 errors,
  only the pre-existing value.cpp warning. Saved at
  `/tmp/build_errors_108b/full.err`.

- Sub-phase 10.8c (wavetable family, marker total 39 → 25):

  * **wavetable_ir.cpp (5 → 0)**:
    - Resolved the WavetableIR ctor copy from `front.manager_`: the
      8-byte `movsd/movlps` at 0x29ce5d-0x29ce81 is a combined copy of
      `(lineNr_, waveformCounter_)` (two consecutive int32 fields at
      WavetableManager+0..+8), NOT a `samplesPerWave` double. Source
      now reproduces the field-by-field copy.
    - Resolved cancelCallback `controlBlock()` non-API call: the binary
      inlines libc++ `weak_ptr::lock()` (read ctrl block at +0xC0,
      conditional `__shared_weak_count::lock()`, then read raw at +0xB8).
      Reproduced as the public `weak_ptr::lock()` API call. Same pattern
      as the 10.8a Node weak_ptr refactor.
    - Resolved `getUniqueName` "not a member": it's a free function in
      the anonymous namespace at 0x2a0fd0, already used in three other
      wavetable files. Added a forward declaration; call site now
      builds the unique name from `manager_->lineNr_` and a
      post-incremented `manager_->waveformCounter_` (matches binary at
      0x29ea60).
    - Resolved `loadWaveform` field check + branch logic: source had
      the empty-vs-nonempty branch inverted AND wrapped the body in a
      try/catch that the binary does not have. Verified at 0x29f310:
      check is `Waveform::waveformType != 0` (skip if loaded), then
      `signal.checkAllocation()`, then load only when
      `signal.samples_` is empty. Source rewritten to match.
    - Resolved missing `CsvParser` declaration: added a minimal
      forward-declared `CsvParser` class with the static template
      `csvFileToWaveform<WaveformIR>(shared_ptr, AwgDeviceType)`
      method (symbol at 0x2be830). Full reconstruction is in Phase 12.

  * **wavetable_manager_ir.cpp (4 → 0)**:
    - Discovered WaveformIR DOES have a third constructor taking
      `(string, File::Type, DC&)`. The binary at 0x2a9fe0 calls
      `std::allocate_shared<WaveformIR>` whose dispatcher (0x2aa170)
      inlines that ctor body. Added the constructor declaration to
      `waveform_ir.hpp`.
    - Replaced the speculative `make_shared<Waveform>()` workaround in
      `WavetableManager<WaveformIR>::newWaveform` with a proper
      `std::allocate_shared<WaveformIR>(allocator, name, kType, dc)`
      call.
    - Documented the 16-byte `movups xmm0` block copy at 0x2aa073 as
      covering Signal+0x48..+0x57 (channels_, reserveOnly_, padding,
      length_low). Reproduced field-by-field for readability.
    - Added the source-vs-dest identity guard on `secondaryName` to
      match the binary's `cmp rdi, r15` at 0x2aa088.

  * **waveform_ir.hpp (3 → 1 documentation; semantic markers all
    resolved)**:
    - Renamed `irBool1` to **`crossesCacheLine_`**. Verified by
      enumerating all writers (0x2a9e47 sets to 1 in
      WavetableIR::allocateWaveforms's filler branch; 0x2aa88b /
      0x2ad023 copy bit 8 of MemoryAllocator block flags;
      0x2ad0ac clears to 0 in the reload-without-CL-crossing path).
      Updated layout comment, struct field, struct_layouts.md, and
      the 5 wavetable_ir.cpp call sites + waveform_ir.cpp ctor inits.
    - Caught and fixed an unrelated mislabelling in
      `prefetch.cpp:742`: a comment claimed `+0xDA / irBool1` but
      the disasm at 0x1ccb7f reads `cmp [rax+0xd8], 1` →
      `markedForLoad`. Source now uses the correct field.
    - The remaining "Speculative fields removed" banner is documentation
      of prior cleanup work, not an active TODO; reworded to avoid
      grep false-positives.

  * **waveform_ir.cpp (1 → 0)**:
    - Resolved the marker-bits loop in `WaveformIR::toJsonElement`.
      Source was reading `signal.samples_.data()` cast to `uint8_t*`
      (semantically wrong); binary at 0x2c571b reads from WaveformIR
      +0xB0/+0xB8 = `signal.markerBits_._begin_/._end_`. Now uses
      `signal.markerBits_.data()` directly.

  * **wavetable_manager_front.cpp (1 → 0)**:
    - Removed stale TODO on `Value::operator==`: that operator IS
      declared at value.hpp:155 (symbol 0x21a780). Code already
      compiled; comment was outdated.

  * **waveform_front.hpp (1 → 0 documentation)**:
    - Reworded "Hallucinated TBD fields removed" banner to "Speculative
      fields removed" — same content, no marker false-positive.

  Methodology notes:
   * Every renamed field was verified against ALL writers/readers via
     symbol-grep before committing. The `irBool1 → crossesCacheLine_`
     rename incidentally exposed a separate +0xD8/+0xDA confusion in
     prefetch.cpp that would otherwise have remained latent.
   * The `WaveformIR(string, File::Type, DC&)` ctor finding overturns
     the prior assumption that the IR class had only two ctors. The
     evidence (allocate_shared dispatcher inlining the ctor body) is
     consistent with how libc++ `__shared_ptr_emplace` works for
     non-trivially-default-constructible payloads.
   * Detection of the inverted branch in `loadWaveform` is a useful
     reminder: when a `je` jumps INTO a function body (not OUT), the
     conditional's polarity is the OPPOSITE of "jump on equal means
     equal-case is the rare path". Always trace the destination.

  Build status after 10.8c: full -fsyntax-only clean, 0 errors.
  Marker count: 39 → 25 (-14).

- Sub-phase 10.8d (long tail across src/ + include/, marker total 25 → 9):
  Walked every remaining real-code marker. Resolved 16 markers across 7
  files; the 9 left in place are intentional doc banners and
  forward-references to deferred phases (10.7d Exception base, Phase 11
  expression/seqc_ast_node).

  Files touched (resolutions):

  1. `asm_parser_context.cpp:342` — Replaced "up to first space" TODO with
     `name.find(' ')` + substr (verified `memchr(buf, 0x20, len)` at
     0x28c712-0x28c75a).

  2. `static_resources.cpp` (3 markers + new `errorReportTarget()`):
     * Line 80: Enabled deprecated-constant warning emission. Order
       verified at 0x129ed1-0x12a256: (1) check `DEVICE_SAMPLE_RATE`
       special, (2) `Resources::getVariable`, (3) post-check name against
       three deprecated constants, calling `std::function` at this+0x100
       with `ErrorMessageT(0x34)`. Added `errorReportTarget()` accessor
       on StaticResources to expose the inline std::function (libc++ ABI:
       0x18-byte inline functor + ptr at +0x100). Added `extern const
       std::string` declarations for `constAwgIntegrationTrigger`,
       `zsyncDataPqscRegister`, `zsyncDataPqscDecoder` (BSS @
       0xb84690/0xb846a8/0xb846c0).
     * Line 201: device-subtype mask was hard-coded `if(true)`. Disasm
       at 0x1ed136 uses `bt rdx, rcx` with bitmask `0x100010100`,
       selecting deviceTypes {0, 8, 16, 32}. Replaced with explicit
       mask check.
     * Line 322: hard-coded sample rate `2.4e9` was WRONG. Disasm at
       0x1f0618 proves the binary SKIPS the `addConst` entirely when
       `deviceType==2 && isnan(rate)` — it does not substitute a literal.
       Restructured with explicit `emitSampleRate` flag.

  3. `prefetch_emit.cpp` + `prefetch.hpp` + `prefetch_placesingle.cpp`
     (3 emit markers + 6 callsite updates):
     * **`findPlaceholder` return-type refactor**: changed from
       `shared_ptr<Node>` → `AsmList::Asm*`. Disasm at 0x1d6b50 shows a
       linear scan over the `AsmList` vector with stride 0xA8
       (sizeof(Asm)) returning a raw element pointer. Closes
       `unknowns.md` item #82.
     * Required `prefetch.hpp` to `#include "asm_list.hpp"`.
     * `prefetch_placesingle.cpp` adapted to the new API: introduced a
       `placeholderIter()` lambda that converts `Asm*` → vector iterator
       by storing the index and recomputing on demand (vector may
       reallocate between insertions). Re-enabled the previously-blocked
       `setWavetableFrontIndex(placeholder->wavetableFront)` line by
       adding a public accessor pair on `AsmCommands` (the field is
       private). Converted one legacy `insert(node, AsmList&)` callsite
       to use the iterator-range form.
     * Removed dead `clampToCache` duplicate body in
       `prefetch_helpers.cpp` (canonical impl lives in
       `prefetch_emit.cpp`).

  4. `prefetch_prepare.cpp` (3 markers):
     * Line 136 was a stale TODO (code already locks the weak_ptr
       correctly).
     * Line 161 (stack/deque mismatch): the symbol attached to
       `removeBranches` at the call site (0x1c8cd7) is unambiguously
       `std::stack<shared_ptr<Node>, std::deque<shared_ptr<Node>>>&`.
       Therefore the `prepareTree` traversal local — passed via `rdx`
       at that call — IS a `std::stack`, NOT a raw `std::deque`. Changed
       the local declaration and translated all 5 in-function operations
       (`push_back/pop_back/back` → `push/pop/top`). Re-enabled the
       `removeBranches(current, stack)` call. (The `countBranches` and
       `definePlaySize` traversal locals were left as `std::deque` —
       they are not passed to any stack-typed callee, so the disasm
       evidence is ambiguous and changing them would be speculative.)
     * Line 660: `PlayConfig::unknown_1e` doesn't exist because the
       field IS already defined as `PlayConfig::dummy` at +0x1E
       (`play_config.hpp:40`). Offset 0x66 = 0x48 (Node::config) + 0x1E.
       Enabled the conditional.

  5. `prefetch_helpers.cpp` (3 markers):
     * Line 223: `clampToCache` duplicate `#if 0` body — removed entirely
       (canonical impl in `prefetch_emit.cpp`).
     * Line 325: `branchesModified` was a hallucinated field name —
       **the actual `+0x109` field IS `Node::branchMaySkipAllBodies`**
       (already defined in `node.hpp:297`). Verified at 0x1d3748:
       `mov BYTE PTR [r14+0x109], 0x1`. Enabled the assignment.
     * Line 539: `slotIndex/data[]/hasValue` TODO was stale — the
       existing `wavesPerDev[deviceIndex]` code is correct. Verified
       at 0x1d6277: stride 0x20 = sizeof(optional<string>), `has_value`
       at +0x18, SSO/long string at +0x00..+0x17. Removed the TODO
       comment and added a citation comment.

  6. `awg_assembler_impl_pipeline.cpp` + `awg_assembler_impl.hpp` +
     `awg_assembler.hpp` + `awg_assembler.cpp` + `awg_assembler_impl.cpp`
     (2 pipeline markers, 1 type correction across 4 files):
     * Line 323 (Label ctor or addLabel?): disasm at 0x287e33 shows
       the binary constructs a *local* `AsmParserContext::Label{
       labelCounter, labelStr}` with no further use beyond holding the
       string until it's moved into `expr->labelName`. There is no
       `addLabel`/`hasLabel`-set registration here. Removed the
       erroneous `parserCtx_.addLabel(labelStr)` call; documented
       temporary-Label semantics. The labelName/hasLabel/labelIndex
       assignment block at lines 327-338 was already correct: r14 is
       the `make_shared` control block, so `[r14+0x70]/+0x78/+0x90]` =
       AsmExpression+0x58/+0x60/+0x78 = labelIndex/labelName/hasLabel.
     * Line 575: **`opcodes_` field type was wrong** — declared as
       `vector<uint64_t>`, but disasm at 0x2885cf-0x2885da passes
       `&opcodes_` (`[r14+0x50]`) directly to `ElfWriter::addCode(
       vector<uint32_t> const&)`. Therefore `opcodes_` IS a
       `vector<uint32_t>`. Updated 4 files: field type in
       `awg_assembler_impl.hpp`, `getOpcode()` return type in both
       `awg_assembler_impl.hpp` and `awg_assembler.hpp`, plus the two
       impl signatures in `awg_assembler_impl.cpp` and
       `awg_assembler.cpp`. Removed the spurious memcpy adapter in
       `writeToFile()` — `addCode(opcodes_)` is now called directly.
     * Note on layout invariance: `sizeof(std::vector<T>)` is 0x18 on
       libc++ regardless of `T`, so no offsets in the surrounding
       struct shifted; the change is purely semantic.

  Methodology highlights:
  * Multiple "TBD field" markers turned out to refer to fields that
    were already defined under different names. Always grep the header
    for the documented offset BEFORE assuming a field is missing
    (saved invasive struct edits in 3 cases this sub-phase:
    `branchMaySkipAllBodies`, `PlayConfig::dummy`, and the
    `wavesPerDev` lookup pattern).
  * The container-type ambiguity in `prepareTree` was resolved by
    inspecting the *symbol* attached to the callee at the cross-TU
    call site — the demangled C++ symbol carries the full container
    template arguments, which are otherwise invisible from raw
    disassembly.
  * Refactoring the public return type of a libc++ container
    (`vector<uint64_t>` → `vector<uint32_t>`) had ZERO impact on
    layout offsets thanks to libc++'s uniform 0x18-byte vector
    representation — but it would have silently corrupted any caller
    that read the data as `uint64_t`. The 4-file batch update was
    necessary even though no caller code path triggered an immediate
    error.

  Build status after 10.8d: full -fsyntax-only clean across all
  src/*.cpp, 0 errors, 0 new warnings (only the pre-existing
  value.cpp placement-new warning remains).
  Marker count: 25 → 9 (-16). All 9 remaining are intentional
  doc/banner notes (6) and forward-references to deferred phases (3:
  10.7d Exception base, Phase 11 expression/seqc_ast_node).

- **Phase 10.7d (zhinst::Exception base class)** — COMPLETE.
  Resolved unknowns.md items 79 (zhinst::Exception layout) and 80
  (zhinst::GenericErrorDescription<T> template). Reconstructed
  `include/zhinst/exception.hpp` and `src/exception.cpp`.

  Verified layout (size 0x60, derived from
  `Exception::~Exception` calling `operator delete(this, 0x60)` at
  0x2e7791):

  | Offset      | Field                              | Source                       |
  |-------------|------------------------------------|------------------------------|
  | +0x00       | std::bad_exception vptr (primary)  | vtable b09fa0                |
  | +0x08       | boost::exception vptr (secondary)  | vtable b09fc8                |
  | +0x10       | boost_data_ ptr (refcounted)       | dtor `[rbx+0x10]` release    |
  | +0x18       | boost throw_function_              | init 0                       |
  | +0x20       | boost throw_file_                  | init 0                       |
  | +0x28       | boost throw_line_ (sentinel)       | init -1                      |
  | +0x30..+0x47| errorCode_ (boost::system::error_code, 24B) | code()/description() |
  | +0x48..+0x5F| message_ (libc++ std::string SSO)  | what() reads SSO bit @ +0x48 |

  GenericErrorDescription<T> is a 48-byte `{T(24B), std::string(24B)}`
  pair, verified at the (GenericErrorDescription) ctor 0x2e5810 by
  observing the parameter access pattern (xmm[+0x00], q[+0x10] ->
  errorCode_; xmm[+0x18], q[+0x28] -> message_, with source nulled out
  to mark moved-from).

  All 5 ctors verified:
  * 0x2e5410 default       — hardcodes message "ZIException"
  * 0x2e54b0 (string)      — moves string in, falls back to
                              `ErrorCodeTraits::defaultMessage()` if empty
  * 0x2e55b0 (error_code)  — formats `code.to_string()` with prefix at
                              .rodata 0x90c6c6
  * 0x2e5700 (error_code, string) — uses string verbatim
  * 0x2e5810 (GenericErrorDescription) — pure move-construct

  All 3 observers verified:
  * 0x2e5870 `what()`        — equivalent to `message_.c_str()`
                                (manual SSO/heap branch on `[+0x48]&1`)
  * 0x2e5890 `code()`        — returns errorCode_ by value (sret)
  * 0x2e58b0 `description()` — returns `&errorCode_` (the binary's
                                "description" name was misleading;
                                it's really `&errorCode_`, not the
                                message)

  Derived class survey (28 derived types found at 0x2e58c0..0x2e76xx):
  ZIAPIException, ZIIOException, ZIDeviceException, ZISocketException,
  ZIOverflowException, ZIUnderrunException, ZITimeoutException,
  ZIReadOnlyException, ZIWriteOnlyException, ZINotFoundException,
  ZIInvalidKeywordException, ZITypeMismatchException,
  ZIOutOfRangeException, ZIInterruptException, ZIInternalException,
  ZIDeviceNotVisibleException, ZIDeviceNotFoundException,
  ZIDeviceInUseException, ZIDeviceInterfaceException,
  ZIDeviceConnectionTimeoutException,
  ZIDeviceDifferentInterfaceException, ZIDeviceFWException,
  **ZIAWGCompilerException**, **ZIAWGOptimizerException**,
  ZIVersionException, ZIIllegalPathException.

  Of these, only `ZIAWGCompilerException` and `ZIAWGOptimizerException`
  are constructed by reconstructed seqc compiler source. Both add
  zero data fields — they differ from the base only by vtable identity
  (and thus the per-class hardcoded class-name string in the no-arg
  ctor). They are both fully reconstructed in the new
  `exception.hpp`/`exception.cpp`. The other 24 derived types are
  documented in the header but not declared, to be added on demand
  if/when seqc compiler code paths construct them.

  Source-level approximation choices (documented in exception.hpp):
  * The MI base (`std::bad_exception` + `boost::exception`) cannot be
    cleanly expressed in portable source without pulling in
    `<boost/exception/exception.hpp>`. We derive Exception from
    `std::exception` only — semantically `std::bad_exception` already
    derives from `std::exception`, so all `catch(std::exception&)` call
    sites in src/ behave identically.
  * `boost::system::error_code` is approximated by a 24-byte POD
    `zhinst::ErrorCode` with `value_`, `category_`, `source_` fields.
    Total size matches the binary; the precise internal split is
    irrelevant since no src/ code introspects its bits.
  * The boost::exception data slot is preserved as 4 trailing fields
    (boost_data_/boost_throw_function_/boost_throw_file_/boost_throw_line_)
    so the visible layout — and the offset of message_ at +0x48 — is
    correct relative to where the binary would put it.

  Cleanup: `error_messages.hpp` no longer carries the local stub
  `class ZIAWGCompilerException : public std::exception`. It now
  `#include`s `exception.hpp` for the canonical declaration. The
  outdated forward-reference comment about Phase 10.7d in
  `error_messages.hpp` (the source of marker count item that used to
  appear in this file) is replaced with a brief "see exception.hpp"
  pointer.

  Build status after 10.7d: clean cmake build (0 errors, 0 warnings)
  via `cmake --build .` — verified linker-level (no
  undefined-symbol issues for the new Exception ctors/dtor/observers).
  Marker count: 9 → 8 (-1: removed forward-ref in error_messages.hpp).

- **Phase 10.7e (ErrorMessageT renumbering)** — COMPLETE.
  All 253 SeqC-range ErrorMessageT entries (keys 0..254) were off by +1
  from the binary's map keys; renumbered in-place.  Gap annotations corrected
  (real binary gaps at keys 47/0x2F and 53/0x35, not at the old positions).
  16384+, 32768+, 36864+ ranges unaffected.
  Build: clean (0 errors, 0 warnings).

- **Phase 11a (Expression struct + parser actions)** — COMPLETE.
  Expression is a plain struct (no vtable), 0x58 bytes (libc++), managed via
  shared_ptr.  Layout:
    +0x00  int32  operationType (EOperationType: 0=Command..12=Value)
    +0x04  int32  valueCategory  (0 or 2=rvalue for Value/String/FunctionCall)
    +0x08  int32  lineNumber
    +0x0C  int32  (padding)
    +0x10  double value
    +0x18  string name           (24B libc++ SSO)
    +0x30  vector<shared_ptr<Expression>> children (24B)
    +0x48  int32  operator_      (EOperator: 0..21)
    +0x4C  int32  commandType    (ECommandType: 0..16)
    +0x50  int32  varType
    +0x54  int32  valueType      (always 2)

  Three enums recovered from binary str() functions with jump tables:
    EOperationType (13 values) — str() @0x1c1530, jump table @0x95a82c
    EOperator (22 values)      — str() @0x1c1790, jump table @0x95a860
    ECommandType (17 values)   — str() @0x1c18e0, jump table @0x95a8b8

  Reconstructed functions (24 total):
    - Expression copy ctor @0x1bfa30
    - copyExpression @0x1bef20 (deep recursive copy)
    - createValue @0x1bf260, createString @0x1bf2d0, createVariable @0x1bf420
    - addVariableType @0x1bf560, createVariableType @0x1bf7c0
    - createOperator @0x1bf830, createAssignOperator @0x1bf9c0
    - createArray @0x1bfb50, createListType @0x1bfb70
    - createOrAppendListType @0x1bfd20 + 4 wrappers (ArgList/DeclList/ParamList/StmtList)
    - createFunctionCall @0x1bfe60, createFunction @0x1c0000
    - createCommand @0x1c0330 (variadic)
    - createIf/IfElse/Switch/Case/CondExpression/For/While/Repeat/DoWhile
    - seqc_error @0x2ca1b0 (bison error callback)
    - str(EOperationType), str(EOperator), str(ECommandType)

  Files: expression.hpp, expression.cpp, seqc_parser_context.hpp (stub).
  Build: clean (0 errors, 0 warnings).

  Known approximations:
    - pushChild() uses a no-op deleter shared_ptr instead of the binary's
      custom control block (TODO #93).
    - sizeof(Expression) is 0x60 with libstdc++ vs 0x58 with libc++
      (string size difference: 32 vs 24 bytes).

- **Phase 11b (SeqCAstNode hierarchy: layout + base + 53 subclass declarations)**
  — COMPLETE (declarations and base methods only; subclass method bodies
  intentionally deferred — see scope note below).

  SeqCAstNode is a polymorphic base class, 0x18 (24) bytes, vtable at
  .rodata 0xb06618.  Layout:
    +0x00  vptr
    +0x08  EValueCategory  valueCategory  (3 values)
    +0x0C  int             type           (purpose unclear; ctor arg #2)
    +0x10  EParamDirection direction      (3 values; AST-side enum,
                                           disambiguated from the
                                           inferred resources.hpp
                                           EDirection)
    +0x14  4 bytes padding to 0x18

  Base methods (fully reconstructed):
    - Ctor @0x1fda00              SeqCAstNode(EValueCategory, int, EParamDirection)
    - D2 dtor @0x209000           trivial
    - D0 dtor @0x2462e0           ud2 (abstract — never delete via base)
    - swap() @0x1fda40            swaps (vc,type) qword + direction; vptrs unchanged
    - children() @0x1fda20        default returns empty vector
    - getListElements() @0x209dd0 default returns vector<string>{""}
    - printSeqCAst() @0x1fa3c0    free-function pretty-printer wrapper

  53 named subclasses (NOT in an anonymous namespace as TODO.md guessed)
  declared with full member layouts and virtual method signatures.
  Vtable cluster: .rodata 0xb04f60..0xb060c8.  Class sizes derived from
  D0 destructors (which encode size via `mov esi, SIZE; jmp operator delete`):

    Family                                        Size  Examples
    --------------------------------------------  ----  ----------------------------
    Trivial leaf                                  0x18  SeqCOperation, SeqCCommand,
                                                       SeqCVariableType, SeqCLabel,
                                                       SeqCContinueStatement,
                                                       SeqCBreakStatement, SeqCNoCmd
    Single-child unary                            0x20  SeqCNeg, SeqCPos, SeqCInv,
                                                       SeqCNotExpr,
                                                       SeqCReturnStatement
    SeqCOperator + 22 binary derivatives          0x28  SeqCPlus..SeqCNoOp
                                                       (vptr-only differences)
    Two-child direct                              0x28  SeqCFunctionCall, SeqCArray,
                                                       SeqCIfCondition, SeqCCaseEntry,
                                                       SeqCSwitchCase, SeqCWhileLoop,
                                                       SeqCDoWhile, SeqCRepeat
    Three-child direct                            0x30  SeqCIfElse, SeqCCondExpr
    Four-child direct                             0x38  SeqCFunction, SeqCForLoop
    List nodes (vector<unique_ptr>)               0x30  SeqCArgList, SeqCDeclList,
                                                       SeqCParamList, SeqCStmtList
    SeqCVariable (string payload)                 0x30  (libc++; 0x38 libstdc++)
    SeqCValue (tagged union; tag@+0x30,           0x38  (variant slot at +0x18)
       jump table @0xb065a0)

  Two new enums:
    EValueCategory (3 values: eNOVALUECATEGORY/eLVALUE/eRVALUE)
        — str() @0x1c16c0
    EParamDirection (3 values: eIN/eOUT/eINOUT)
        — str() @0x1c1730
        — Named EParamDirection (not "EDirection") to avoid colliding
          with the pre-existing inferred enum in resources.hpp.  See
          unknowns.md item #94.

  Files: seqc_ast_node.hpp (488 lines), seqc_ast_node.cpp (355 lines).
  Build: clean (0 errors, 0 warnings).

  Phase 11b SCOPE (user-confirmed): "Layout + base + class declarations
  only".  Base class methods are fully reconstructed; subclass methods
  (clone/print/evaluate/per-node accessors for child slots) are stubbed
  with TODO markers and binary addresses noted in comments.  Full
  per-subclass method-body reconstruction is deferred to a future
  sub-phase (e.g. Phase 11b-extended) in keeping with the
  "structure AND methods together" principle being explicitly relaxed
  here for the 53-class hierarchy.

  Phase 11a follow-up fix: expression.hpp had its own placeholder
  `using VarType = int32_t` which conflicted with the real enum in
  resources.hpp once a second TU dragged both in.  Fixed by including
  resources.hpp in expression.hpp and replacing literal `0` and `3`
  initializers with `VarType_Unset` and `VarType_Const` respectively
  in expression.cpp.

- **Phase 11c (FrontendLowering types)** — COMPLETE.
  Two stack-local structs used by FrontEndLoweringFacade::lower() during
  AST lowering, plus one anonymous-namespace utility function.

  FrontendLoweringContext (0x50 = 80 bytes):
    +0x00  CompilerMessageCollection*    (raw, non-owning)
    +0x08  shared_ptr<AsmCommands>
    +0x18  shared_ptr<CustomFunctions>
    +0x28  shared_ptr<WaveformGenerator>
    +0x38  shared_ptr<WavetableFront>
    +0x48  int32_t channelGrouping
    Dtor @0x1233b0: destroys 4 shared_ptrs in reverse order.

  FrontendLoweringState (0x30 = 48 bytes):
    +0x00  shared_ptr<void>   (result — type TBD, copied to lower() return)
    +0x10  uint64_t           (zeroed — bool/padding)
    +0x18  vector<string>     (accumulator)
    Dtor @0x1c2190: destroys vector<string> then shared_ptr.

  constWaveform @0x22c9f0 (anon namespace, 0x370 bytes):
    Signature: shared_ptr<EvalResults> constWaveform(int, double,
               FrontendLoweringContext&).
    Creates empty EvalResults (0x80 bytes), builds vector<Value>{int, double},
    calls WaveformGenerator::eval("rect", values).  Catches two exception
    types → errorMessage("WaveformGenerator [Value] Exception", -1).
    Body left as documented stub (requires WaveformGenerator::eval decl and
    EvalResults layout).

  Discovery: FrontEndLoweringFacade::lower() is NOT void — it returns a
  16-byte struct (likely shared_ptr) via sret, stored into Compiler::ast_
  (+0x28).  Current compiler.hpp declaration is wrong (says void).
  See unknowns.md item #97.

  Files: frontend_lowering.hpp, frontend_lowering.cpp.
  Build: clean (0 errors, 0 warnings).

- **Phase 11d (CustomFunctions)** — COMPLETE (layout + declarations + 12 methods).
  CustomFunctions is the main dispatch hub for ~80 SeqC built-in functions
  (playWave, wait*, set*, get*, etc.). Size: 0x1E0 (480 bytes).

  New types introduced:
    EvalResultValue (0x38 = 56 bytes) — Value-like variant passed to all builtins.
    NodeMapItem (0x18 = 24 bytes) — hashable node map key with polymorphic data.
    NodeMapData (base) / VirtAddrNodeMapData / DirectAddrNodeMapData — polymorphic
      node address representations.
    AccessMode — enum used in set<AccessMode> for node access tracking.
    MathCompiler — inline sub-object (0x30 bytes at +0xC8) with math function dispatch.
    MathCompilerException — exception for math errors.
    CustomFunctionsException (0x20 bytes) — generic exception with string message.
    CustomFunctionsValueException (0x40 bytes) — exception with argIndex + varName.
    PlayArgs — argument parser for play functions.
    SubFunc — enum for play()/playIndexed() dispatch variants.

  CustomFunctions member layout (0x1E0 bytes):
    +0x00  AWGCompilerConfig const*              config_
    +0x08  DeviceConstants const*                devConst_
    +0x10  shared_ptr<Resources>                 resources_ (lazy, set later)
    +0x20  shared_ptr<WavetableFront>            wavetableFront_ (from ctor)
    +0x30  shared_ptr<WavetableFront>            wavetableFrontPrivate_ (new'd in ctor)
    +0x40  shared_ptr<WaveformGenerator>         waveformGen_
    +0x50  shared_ptr<AsmCommands>               asmCommands_
    +0x60  unordered_map<string, function<...>>  funcMap_ (dispatch table)
    +0x80  float (1.0f)                          field_80
    +0x88  unordered_map<string, vector<string>> field_88
    +0xA8  float (1.0f)                          field_A8
    +0xB0  set<string>                           field_B0
    +0xC8  MathCompiler (0x30)                   mathCompiler_
    +0xF8  map<string, NodeMapItem>              nodeMap_ (lazy-init)
    +0x100 unordered_map<NodeMapItem, uint32>    nodeAddressMap_
    +0x128 unordered_map<NodeMapItem, set<AM>>   accessModeMap_
    +0x150 vector<NodeMapItem>                   nodeList_
    +0x168 unordered_map<?, ?>                   field_168 (unknown)
    +0x190 function<void(string const&)>         warningCallback_
    +0x1C8 set<string>                           usedFeatures_

  12 methods fully analyzed and implemented:
    checkPlayMinLength, checkPlayAlignment, oscMaskCheckGrimsel,
    oscMaskCheckHirzel, oscMaskSetAllHirzel, oscMaskSetAllGrimsel,
    addNodeAccess, getWaitTime, initNodeMap, getNodeAddress,
    getSampleClock, getAccessModes.

  ~80 SeqC built-in function bodies stubbed with addresses in comments.
  Exception classes (CustomFunctionsException, CustomFunctionsValueException)
  fully implemented.

  Files: custom_functions.hpp, custom_functions.cpp.
  Build: clean (0 errors, 0 warnings).

- **Phase 12a (WaveformGenerator)** — COMPLETE (layout + 3 methods + 2 exception classes).
  WaveformGenerator is the waveform DSP function registry, containing ~35 named
  waveform generation functions (zeros, ones, sine, cosine, sinc, ramp, sawtooth,
  triangle, gauss, drag, blackman, hamming, hann, rect, chirp, mask, marker, rand,
  randomGauss, randomUniform, lfsrGaloisMarker, rrc, vect, placeholder, join, add,
  interleave, scale, multiply, cut, flip, filter, circshift, merge, grow).

  Class size: 0xC0 (192 bytes). Confirmed from shared_ptr_emplace dealloc 0xE0.
  Ctor @0x248200, Dtor @0x127840.

  WaveformGenerator member layout (0xC0 bytes):
  | Offset | Type | Name |
  |--------|------|------|
  | +0x00 | unordered_map<string, function<Signal(vector<Value> const&)>> | funcMap_ |
  | +0x20 | float (1.0f) | field_20_ |
  | +0x28 | unordered_map<string, string> | aliasMap_ (deprecated→current name map) |
  | +0x48 | float (1.0f) | field_48_ |
  | +0x50 | set<string> | field_50_ |
  | +0x68 | shared_ptr<WavetableFront> | wavetableFront_ |
  | +0x78 | uint64_t | field_78_ (unknown) |
  | +0x80 | function<void(string const&)> | warningCallback_ |
  | +0xB0 | shared_ptr<void> | field_B0_ (null initially) |

  Implemented methods: createDummyWaveform, genericTriangle, reverse.
  Stubbed: ~35 waveform functions + call, eval, getOrCreateWaveform,
  readDouble, readDoubleAmplitude, readInt, readPositiveInt, readWave,
  markerImpl, interpolateLinear, functionExists.

  Exception classes:
    WaveformGeneratorException (0x20 bytes) — vptr + string message_.
    WaveformGeneratorValueException (0x28 bytes) — vptr + string message_ + size_t argIndex_.
  Both fully implemented (ctor, dtor, what).

  Key discovery: funcMap_ keys are "sine"/"cosine" (not "sin"/"cos"), while the
  C++ method names are sin()/cos(). aliasMap_ maps deprecated names to current
  names; call() checks aliasMap_ first and emits a warning via warningCallback_.

  Files: waveform_generator.hpp, waveform_generator.cpp.
  Build: clean (0 errors, 0 warnings).

- **Phase 12b (CachedParser)** — COMPLETE (nested types + 8 methods stubbed).
  CachedParser is an on-disk LRU cache for parsed waveform files, using
  Boost.Serialization (text_iarchive/text_oarchive) for index persistence.

  Class size: 0x60 (96 bytes). Ctor @0x2afa70, Dtor @0x29aac0.

  Nested types:
    CacheEntry (0x60 bytes): name_ + filePath_ + fileSize_ + timestamp_ +
      hash_ (vector<uint>) + valid_ (bool). Ctor @0x2b10b0, dtor @0x2b1320,
      operator= @0x2b1210.
    CachedFile (0x50 bytes): found_ (bool) + samples_ (vector<double>) +
      markers_ (vector<uint8_t>) + markerBits_ (vector<uint8_t>).
      Dtor @0x2b1f70.

  Methods (8 total, all stubbed with addresses):
    - CachedParser ctor/dtor (reconstructed logic)
    - loadCacheIndex @0x2afec0 (stub — boost deserialization)
    - saveCacheIndex @0x2b03c0 (stub — boost serialization)
    - cleanCache @0x2b0140 (approximate logic)
    - removeOldFiles @0x2b01a0 (stub)
    - cacheFile @0x2b05b0 (stub)
    - cacheFileOutdated @0x2b14d0 (stub)
    - getCachedFile @0x2b1900 (stub with correct return type)
    - getHash @0x2b1fe0 (reconstructed — delegates to util::wave::hash)

  Files: cached_parser.hpp, cached_parser.cpp.
  Build: clean (0 errors, 0 warnings).

See `reconstructed/notes/` for detailed opcode tables, struct offset maps, and the full list of open questions.
