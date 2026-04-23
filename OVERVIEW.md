# Overview: Reconstructed zhinst SeqC Compiler Assembler Backend

Reverse-engineered from `_seqc_compiler.so` (ELF 64-bit, x86-64, not stripped).

> **Detailed phase-by-phase reconstruction notes (Phases 1-12) archived in
> [`reconstructed/notes/archive/OVERVIEW_phases_1-12.md`](reconstructed/notes/archive/OVERVIEW_phases_1-12.md).**

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

Expression (0x58)                  # Parser-level expression tree node
  └─ 24 create*/copy/str()        # Factory functions from bison actions

SeqCAstNode (0x18 base)            # AST node hierarchy — 53 named subclasses
  └─ lower() virtual               # → FrontendLowering dispatch

CustomFunctions (0x1E0)            # SeqC built-in function registry (~80 functions)
  ├─ NodeMapItem (0x18)            # Node address descriptor
  ├─ EvalResultValue (0x38)        # Evaluation result variant
  └─ MathCompiler (0x30)           # Embedded math expression compiler

WaveformGenerator (0xC0)           # Waveform DSP function registry (~35 functions)
  └─ WaveformGenerator*Exception   # Value + general exception classes

CachedParser (0x60)                # On-disk LRU cache for parsed waveforms
  ├─ CacheEntry (0x60)             # name + path + size + timestamp + hash + valid
  └─ CachedFile (0x50)             # found + samples + markers + markerBits

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

Exception (0x60)                   # MI: std::bad_exception + boost::exception
  └─ ZIAWGCompilerException        # Compiler-specific exception

CompilerMessage (0x20)             # type + lineNr + message
CompilerMessageCollection (0x20)   # vector<CompilerMessage> + state
CompilerException (0x20)           # std::exception + message string
```

## Reconstruction Status

**44 headers**, **54 source files**, **0 build errors**, **0 warnings**.

Phases 1-12 complete; Phase 13a, 13c, 13d, 13e, 14a complete. All class layouts
verified from disassembly. Method bodies range from fully reconstructed to
stubbed-with-addresses.

### Per-file stub/TODO counts (files with >0 markers)

| File | Stubs | Field unknowns | Approximate logic |
|------|-------|----------------|-------------------|
| `src/seqc_ast_node.cpp` | 1 | 0 | 2 |
| `src/custom_functions.cpp` | 27 | 1 | 3 |
| `src/cached_parser.cpp` | 1 | 0 | 0 |
| `src/waveform_generator.cpp` | 30 | 1 | 1 |
| `include/zhinst/custom_functions.hpp` | 0 | 6 | 0 |
| `include/zhinst/seqc_ast_node.hpp` | 1 | 1 | 0 |
| `src/asm_optimize.cpp` | 1 | 0 | 2 |
| Other files (scattered) | 0 | 0 | ~3 |
| **Total** | **61** | **9** | **~11** |

Phase 13a resolved 106 stubs → 1 remaining (SeqCValue::clone has a
shallow-copy caveat for long strings; needs full variant interface).

Phase 13c resolved infrastructure + 6 representative DSP functions in
WaveformGenerator: call(), getOrCreateWaveform(), createDummyWaveform(),
readInt/readDouble/readDoubleAmplitude/readPositiveInt, plus
zeros/ones/rect/scale/add/gauss. eval() body is documented but throws
"blocked on EvalResults" pending Phase 15a layout work. 27 remaining DSP
stubs got accurate signature-documenting comments. Header corrections:
4 return types fixed (call/eval/getOrCreateWaveform/createDummyWaveform
return shared_ptr<WaveformFront>/shared_ptr<EvalResults>, not Signal);
field_50_ → createdNames_; aliasMap_ confirmed empty; field_B0_ deferred.

Phase 13d resolved all 7 CachedParser methods + 2 BONUS methods
(cleanCache, cacheFileOutdated) + the CacheEntry::serialize template.
loadCacheIndex/saveCacheIndex are reconstructed with their boost-archive
operations as documented placeholders (avoid heavy boost::archive include
churn). cacheFile body is a documented skeleton (ElfWriter calls); the
other methods are full implementations. Layout corrections: CachedFile
has NO `found_` bool — proper layout is `uint16_t channel_; vector<uint8_t>
markerBits_; vector<double> samples_; vector<uint8_t> markers_`. Map size
fields cacheSize_/currentSize_ are BYTES not entry counts. valid_ flag
acts as in-use pin (set true on read access, prevents eviction). New
header: `elf_reader.hpp` (minimal forward decl for ElfReader/ElfSection,
since cacheFileOutdated and getCachedFile both use it). **Phase 14d
upgraded `elf_reader.hpp` to a full reconstruction (0x98 bytes, real
private `ELFIO::elfio` base, ElfException class, `sectionAsString()`
helper) and added `src/elf_reader.cpp` with all 5 methods +
ElfException ctor/dtor/what; cached_parser.cpp call sites switched
from the fictional `getDataAsString()` to `sectionAsString()`.**

Phase 13e re-audited CustomFunctions layout end-to-end. **HISTORICAL NOTE
to prevent regression**: an earlier session misattributed the dtor at
**0x1306c1** (which is in `pybind11::detail::internals` or similar, NOT
CustomFunctions) and derived several wrong field types from it. The real
`CustomFunctions::~CustomFunctions` is at **0x127c90** and the real ctor
is at **0x12bcf0**. Corrections applied: +0xF8 is `unique_ptr<NodeMap>`
(not inline `map<string,NodeMapItem>`); +0x168 is `vector<T>` with 8B
trivially-destructible T (not `unordered_set<string>`, T still TBD).
MathCompiler internal layout resolved (#102): two `std::map`s at +0x00
(single-arg) and +0x18 (multi-arg). DirectAddrNodeMapData has
`uint32_t addr_` at +0x08 (#104). VirtAddrNodeMapData fields deferred
to Phase 14a.

Phase 14a fully reconstructed MathCompiler (33 zhinst:: methods): the ctor
at 0x1c2250 populates both maps with 23 single-arg + 5 multi-arg `std::bind`
emplaces; `call(name, args)` and `functionExists(name, argCount, strict)`
have correct dispatch logic with `MathCompilerException` throws via
ErrorMessages codes 136 (FuncSingleArg), 137 (FuncExactly2Args), 216
(UnknownFunction). Single-arg wrappers map directly to `<cmath>` functions.
**HISTORICAL CORRECTION (cascading from 13e)**: the Phase 13e claim that
field_168 was a `vector<T>` was itself wrong — the dtor at 127cf2 has a
node-walk loop at 127d40-127d70 reached via the `jne` at 127cf0 that the
13e analysis missed. Real type is `std::unordered_set<std::string>` (40B
container, 40B node = 16B header + 24B std::string), confirmed by 1.0f
max_load_factor at +0x188 in ctor and string-dtor pattern in node loop.
Original Phase 11d classification was correct after all. Also resolved
VirtAddrNodeMapData (0x38): vptr + `std::string name_` at +0x08 +
`std::vector<int32_t> addresses_` at +0x20 — confirmed by copy ctor
calling `vector<int>::__throw_length_error` and getJson writing the "name"
key from the string field. Both #101 and #104 fully closed.

### Completed phases (summary)

| Phase | Scope | Key deliverables |
|-------|-------|-----------------|
| 1 | AsmCommands | 83 methods, Cervino/Hirzel impls, factory |
| 2 | Embedded types | Value, Immediate, PlayConfig, AssemblerInstr, Node, AsmList, WaveformFront |
| 3 | Foundational types | DeviceConstants, ErrorMessages (305 entries), Signal, AWGCompilerConfig |
| 4 | AWGAssembler | Pimpl + impl (0x170), 6 opcode encoders, full pipeline, ELF output |
| 5 | Waveform complex | Waveform/Front/IR, WavetableManager\<T\>, WavetableFront (0x200), WavetableIR (0xC8) |
| 6 | AsmOptimize | 0xA0 optimizer, 11 passes, graph-coloring register allocator |
| 7a | Compiler pipeline | Compiler (0x138), full pipeline flow, CompilerMessage/Exception |
| 7b | Cache | Cache (0x28), gap-finding allocator, Pointer state machine |
| 7c-d | Prefetch | 0x160, 30+ methods across 7 files (~5000 lines), tree prep/optimization/emission |
| 8 | Cleanup | MemoryAllocator (0x70), Callbacks, error audit (5701→0 errors) |
| 9 | AWGAssembler completion | ElfWriter (0x78), AsmParserContext, AsmExpression (0xa8) |
| 10 | Scope/resources | Resources/Static/Global, 213 addConst calls, TODO audit (174→70 markers) |
| 10.5-10.8 | Consolidation | Prefetch revisits, hallucination cleanup, marker hotspots (70→10) |
| 11a | Expression | Expression (0x58), 3 enums, 24 parser actions |
| 11b | SeqCAstNode | Base (0x18) + 53 subclass declarations (bodies deferred) |
| 11c | FrontendLowering | FrontendLoweringContext (0x50), FrontendLoweringState (0x30) |
| 11d | CustomFunctions | 0x1E0 layout, 12 methods + ~80 stubs, 8 new types |
| 11e | Compiler fixup | getNodeAccessList/getNodeToModeMap return types |
| 12a | WaveformGenerator | 0xC0 layout, 3 methods + ~35 stubs, 2 exception classes |
| 12b | CachedParser | CacheEntry (0x60), CachedFile (0x50), 8 methods stubbed |
| 13a | SeqCAstNode bodies | All 53 print()+clone() reconstructed, str(VarType/EParamDirection) added |
| 13b | CustomFunctions bodies | Ctor documented (87 registrations), call() with alias resolution, 18 builtins reconstructed |
| 13c-e | Misc bodies | (carried forward) |
| 14a | DeviceType base | DeviceType, DeviceTypeImpl, DeviceOptionSet, DeviceOption/Code/Family enums + toString tables |
| 14b-i | Subclass survey | 14 family-factory classes (Hf2/Mf/Uhf/Hdawg/Shf/Pqsc/Hwmock/Shfacc/Ghf/Qhub/Vhf + Hf2li/Mfli/Mfia subtypes) declared |
| 14b-ii | Per-family bodies | Family ctors, factory makeDefault/doMakeDeviceType impls; initializeSfcOptions per-family parsers; Exception throw-site corrections |
| 14b-iii | AwgDeviceProps | AwgPathPatterns, AwgDeviceProps (0xa0), 9 getAwgDeviceProps<T> specializations, toAwgDeviceType, makeUnsupportedAwgSequencerErrorMessage |
| 14b-iii.5 | DeviceConstants body | All 9 device-type cases (already done in Phase 7e) + corrected default-branch BOOST_THROW_EXCEPTION; jump table @ 0x961aac, XMM constants @ 0x8fc760, sampling-rate doubles documented |
| 14b-iv | Helpers + free fns | getOptionsAsString, expand(DeviceFamily), toStrings(vec<DeviceFamily>), operator<<(ostream,DeviceFamily), allDevices()/makeDevicesSet (flat_set<DeviceTypeCode>), toDeviceOptions exception filter corrected, generateMfSfc body (uint64 stand-in for sfc::FeaturesCode) |
| 14c | Logging + tracing | `zhinst::logging` (Severity, ZiLogger via BOOST_LOG_GLOBAL_LOGGER, LogRecord RAII + error_code instantiation); `zhinst::tracing` (TraceProvider singleton @ .bss 0xb84480, ScopedSpan RAII, getDefaultLabOneResource w/ commit-hash 203353a, makeDefaultSpanProcessor → OTLP/HTTP localhost:31318); OpenTelemetry stub headers under `include/opentelemetry/` for type-checking |
| 14d | ElfReader / ElfWriter | ElfReader corrected to 0x98 bytes (was 0x90 stub) — `formatSection_` (+0x70), `ddSections_` vector (+0x78..+0x90), `pad_` u32 (+0x90); private ELFIO::elfio inheritance; readHeader sentinel checks decoded as `get_class()==ELFCLASS32` && `get_encoding()==ELFDATA2LSB`; getSection throws `ElfException` on miss (was nullptr); ElfException class with `"ELF Exception: "` prefix decoded from 3 movabs immediates; `sectionAsString()` free helper added; ElfWriter already complete from earlier phases |
| 14e | zhinst::sfc namespace | `sfc::FeaturesCode` strong-typed uint64 wrapper added (evidence: source_location literal @0x2deb37 + `rax`-return at 0x2deac1 → ≤8B trivially-copyable); generateMfSfc return type promoted from uint64_t stand-in to `sfc::FeaturesCode`; 6 sfc enum classes (Hf2/Mf/Uhf/Hdawg/Shf/Vhf Option) + OptionCodePair + initializeSfcOptions template were already in place from Phase 14b-iv (13 instantiations confirmed via nm survey) |
| 15a (item #4) | WaveformGenerator field_B0_ | RESOLVED as **negative finding**. No setter exists in binary. The 16B slot at +0xB0 is reserved-but-unused (zero-initialized in WG ctor at 0x2482aa, never written elsewhere). Apparent +0xb0/+0xb8 reads inside WG methods (251973, 25385a, 255068, ...) were misattributed Value-union accesses (Value has type tag at +0xA8 + 16B union storage at +0xB0). The single true +0xb0 write at 0x11d180 is `Compiler::Compiler` writing its own AsmCommands shared_ptr. Closes #110. |
| 15b | Prefetch/Cache deferred items | **Audit-only sub-phase.** All 10 markers from Phase 10.5f were already resolved during post-audit edits on 2026-04-22 (between todo_audit.md @ 01:39 and the prefetch source edits at 04:35-04:53) but never officially closed. Re-audit confirmed zero TODO/FIXME/APPROXIMATE/VERIFY markers remain in prefetch.cpp + prefetch_splitplay.cpp. Disasm-citation density: 301 + 35 lines of `0xNNNN:` cites. Build clean. See `notes/archive/phase_15b_prefetch_audit.md`. |
| 15a-i | Frontend Lowering data model | **EvalResults** class fully reconstructed (0x80B, 7 fields, 14 methods): `values_` (vector\<EvalResultValue\>), `assemblers_` (vector\<AsmList::Asm\>), `hasError_` (bool), `node_` (shared_ptr\<Node\>), `waveformFront_` (shared_ptr\<WaveformFront\>), `name_` (string), `arrayBacking_` (shared_ptr\<EvalResults\>, self-referential). **Value layout corrected** (0x20→0x28): `which_` is at +0x08 not +0x04, `storage_` at +0x10 not +0x08 — 4B alignment padding was missed. **EvalResultValue** fields renamed from opaque to typed (VarType + VarSubType + Value + AsmRegister). **FrontendLoweringState.result** resolved as `shared_ptr<Node>` (was void/TBD). **lower()** return type corrected from void to 32B sret `LowerResult{shared_ptr<Node>, shared_ptr<EvalResults>}` (prior 64B/4-sp claim was wrong). See `notes/frontend_lowering.md`. |
| 15c | AsmOptimize register semantics + algorithms | **Critical correction**: register field semantics were inverted throughout all AsmOptimize methods (reg2=read-src, reg0=write-dest — was backwards). Fixed isRead, isWritten, getNextActionForReg (rewritten), registerIsNeverWritten, registerUpdate, isLabelCalled (iteration direction). **Fully reconstructed** `removeUnusedRegs` @0x27e760 (291 lines). **Structurally reconstructed** `registerAllocation` @0x27ebb0 (1466 lines, 6-phase graph-coloring) and `splitConstRegisters` @0x280440 (444 lines, 2-pass barrier algorithm). Added `AsmRegister::magicSkipRegister()`. Skip bitmask 0x29 pattern documented. Build clean. See `notes/optimization_passes.md`. |

## Key Technical Findings

- **ABI**: Binary uses libc++ (string=24B, function=0x30, __tree=0x30). Build host uses libstdc++ (string=32B). Cannot static_assert sizeof for string-bearing structs. See `notes/libcpp_abi.md`.
- **Return convention**: `const` methods use sret (hidden first param = return pointer). `this` in `rsi`.
- **AwgDeviceType**: 9 devices. Hirzel={HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)}. Cervino={UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)}.
- **Opcodes**: 43 entries in cmdMap. Six instruction encoding formats (opcode0-5).
- **DeviceConstants**: 0x90-byte POD. RegisterBank sub-structs were a misread — all flat scalar fields.
- **Prefetch**: Node+0x18/+0x20 = weak_ptr\<Node\> loadRef (libc++ decomposed). NodeType::Load=1, Play=2 (jump table indices were systematically swapped in early reconstruction).
- **Compiler pipeline**: parse → toSeqCAst → lower → linearize → optimizePreWaveform → serialize/deserialize → runPrefetcher → optimizePostWaveform → unsyncCervino → output.
- **Register allocator**: Graph-coloring with greedy allocation; splits constant-loaded registers on failure.
- **Waveform allocation**: Two-class system — irBool0 partitions waveforms into CL-aligned vs reloading-CL allocation.

## File Structure

```
reconstructed/
├── include/zhinst/
│   ├── types.hpp                # Forward declarations, AwgDeviceType enum
│   ├── awg_compiler_config.hpp  # AWGCompilerConfig (~0xC0), 4 methods
│   ├── asm_register.hpp         # AsmRegister {int value; bool valid} (8B)
│   ├── value.hpp                # Immediate (28B), Value (40B), AddressImpl<T>, ValueException
│   ├── address_impl.hpp         # detail::AddressImpl<T>
│   ├── assembler.hpp            # AssemblerInstr (0x80), Assembler::Command enum (43 opcodes)
│   ├── play_config.hpp          # PlayConfig — shift/mask constants
│   ├── node.hpp                 # Node (0x110), NodeType enum (14 types)
│   ├── waveform.hpp             # Waveform base (0xD8) + WaveformFile (0x40)
│   ├── waveform_front.hpp       # WaveformFront (0xF8)
│   ├── waveform_ir.hpp          # WaveformIR (0xE0)
│   ├── wave_index_tracker.hpp   # WaveIndexTracker (0x28) + WavetableException
│   ├── wavetable_front.hpp      # WavetableFront (0x200) + WavetableManager<Front> (0x48)
│   ├── wavetable_ir.hpp         # WavetableIR (0xC8)
│   ├── wavetable_helpers.hpp    # Wavetable helper declarations
│   ├── asm_list.hpp             # AsmList::Asm (0xA8), AsmList (0x18)
│   ├── error_messages.hpp       # ErrorMessageT enum (305 values), ErrorMessages class
│   ├── asm_commands_impl.hpp    # AsmCommandsImpl base + Cervino/Hirzel
│   ├── asm_commands.hpp         # AsmCommands — 83 method declarations
│   ├── device_constants.hpp     # DeviceConstants (0x90 POD), getDeviceConstants()
│   ├── signal.hpp               # Signal (0x58), SampleFormat, MarkerBitsPerChannel
│   ├── rawwave.hpp              # RawWave hierarchy (PlaceHolder, Hirzel16, Cervino)
│   ├── awg_assembler.hpp        # AWGAssembler pimpl (0x08)
│   ├── awg_assembler_impl.hpp   # AWGAssemblerImpl (0x170)
│   ├── asm_optimize.hpp         # AsmOptimize (0xA0), OptimizeException
│   ├── compiler_message.hpp     # CompilerMessage/Collection/Exception
│   ├── compiler.hpp             # Compiler (0x138), FrontEndLoweringFacade
│   ├── cache.hpp                # Cache (0x28), Pointer (0x24), CacheException
│   ├── memory_allocator.hpp     # MemoryAllocator (0x70), MemoryBlock (0x0C)
│   ├── callbacks.hpp            # CancelCallback, ProgressCallback
│   ├── asm_expression.hpp       # AsmExpression (0xa8) — parse tree node
│   ├── asm_parser_context.hpp   # AsmParserContext (~0x80), Label
│   ├── resources.hpp            # Resources (0xD8), StaticResources (0x110), GlobalResources
│   ├── elf_writer.hpp           # ElfWriter (0x78)
│   ├── prefetch.hpp             # Prefetch (0x160), PrefetcherNodeState
│   ├── expression.hpp           # Expression (0x58), EOperationType/EOperator/ECommandType
│   ├── seqc_parser_context.hpp  # SeqcParserContext stub
│   ├── seqc_ast_node.hpp        # SeqCAstNode base (0x18) + 53 subclass declarations
│   ├── frontend_lowering.hpp    # FrontendLoweringContext (0x50), FrontendLoweringState (0x30)
│   ├── exception.hpp            # zhinst::Exception (0x60), ZIAWGCompilerException
│   ├── custom_functions.hpp     # CustomFunctions (0x1E0), NodeMapItem, EvalResultValue, etc.
│   ├── cached_parser.hpp        # CachedParser (0x60), CacheEntry, CachedFile
│   ├── waveform_generator.hpp   # WaveformGenerator (0xC0) + exception classes
│   ├── logging.hpp              # Severity (8 levels), ZiLogger tag (BOOST_LOG_GLOBAL_LOGGER), detail::LogRecord RAII
│   └── tracing.hpp              # TraceProvider singleton, ScopedSpan RAII, getDefaultLabOneResource, makeDefaultSpanProcessor
├── include/opentelemetry/        # **Stub** umbrella for opentelemetry-cpp (Phase 14c) — type-checks tracing.cpp w/o real SDK
│   └── _stub_fwd.hpp            # nostd::shared_ptr/string_view, common::AttributeValue, trace::Span/Scope/Tracer, sdk::{trace,resource,common}, exporter::otlp
├── src/
│   ├── asm_commands.cpp              # ~550 lines, ~70 of 83 methods
│   ├── asm_commands_impl.cpp         # getInstance() factory
│   ├── asm_commands_impl_cervino.cpp # 11 Cervino virtuals
│   ├── asm_commands_impl_hirzel.cpp  # 11 Hirzel virtuals
│   ├── assembler.cpp                 # cmdMap, string conversions, query helpers
│   ├── asm_list.cpp                  # append, print, serialize, deserialize
│   ├── value.cpp                     # Value, Immediate, AddressImpl — all methods
│   ├── play_config.cpp               # encodeCwvf, operator!=, toJson, fromJson
│   ├── node.cpp                      # 15 methods
│   ├── waveform.cpp                  # 11 methods, 1045 lines
│   ├── waveform_front.cpp            # toString, copy-rename ctor, dtor
│   ├── waveform_ir.cpp               # 2 ctors, toJsonElement
│   ├── wave_index_tracker.cpp        # 7 methods + WavetableException
│   ├── wavetable_front.cpp           # 22 methods
│   ├── wavetable_manager_front.cpp   # 9 methods
│   ├── wavetable_ir.cpp              # 21 methods
│   ├── wavetable_manager_ir.cpp      # 7 methods
│   ├── device_constants.cpp          # getDeviceConstants() — 9 device types
│   ├── error_messages.cpp            # 305 format strings, format<>()
│   ├── signal.cpp                    # 17 methods
│   ├── rawwave.cpp                   # 3 RawWave subclasses
│   ├── awg_compiler_config.cpp       # dtor, string conversions
│   ├── awg_assembler.cpp             # 11 pimpl forwarding methods
│   ├── awg_assembler_impl.cpp        # ctor/dtor, setMemoryOffset, getOpcode
│   ├── awg_assembler_opcodes.cpp     # opcode0-5, getReg, getVal (639 lines)
│   ├── awg_assembler_impl_pipeline.cpp # pipeline + helpers (650 lines)
│   ├── asm_optimize.cpp              # 11 passes, register allocator (837 lines)
│   ├── prefetch.cpp                  # ctor/dtor, globalCwvf, optimize*, allocate
│   ├── prefetch_prepare.cpp          # preparePlays, prepareTree, countBranches
│   ├── prefetch_helpers.cpp          # backwardTree, removeBranches, helpers
│   ├── prefetch_emit.cpp             # placeCommands, fillInPlaceholders (1012 lines)
│   ├── prefetch_print.cpp            # recursive debug printer (573 lines)
│   ├── prefetch_splitplay.cpp        # splitPlay (429 lines)
│   ├── prefetch_placesingle.cpp      # placeSingleCommand (981 lines)
│   ├── compiler_message.cpp          # CompilerMessage/Collection/Exception
│   ├── compiler.cpp                  # Compiler pipeline, parse, compile
│   ├── cache.cpp                     # Cache: allocate, getBestPosition, etc.
│   ├── memory_allocator.cpp          # CL-aware allocator, 3 template instantiations
│   ├── callbacks.cpp                 # ProgressCallback defaults
│   ├── log_exception.cpp             # logExceptionToClog
│   ├── asm_expression.cpp            # 6 factory functions
│   ├── asm_parser_context.cpp        # 22 accessors, addNode, addCommand
│   ├── elf_writer.cpp                # 8 methods
│   ├── elf_reader.cpp                # 5 methods + ElfException (Phase 14d)
│   ├── static_resources.cpp          # ctor (213 addConst calls, ~15KB)
│   ├── global_resources.cpp          # ctor, dtor, TLS init
│   ├── resources.cpp                 # 20+ methods, Variable/Function/State
│   ├── exception.cpp                 # Exception ctors, dtor, what(), description()
│   ├── expression.cpp                # 20+ create* functions, str() for 3 enums
│   ├── seqc_ast_node.cpp             # 53 print/clone bodies, str(EParamDirection), printSeqCAst
│   ├── frontend_lowering.cpp         # constWaveform stub
│   ├── custom_functions.cpp          # 12 methods + ~80 stubs
│   ├── cached_parser.cpp             # CacheEntry/File + 8 method stubs
│   ├── waveform_generator.cpp        # 3 methods + ~35 stubs, 2 exception classes
│   ├── logging.cpp                   # ZiLogger::construct_logger(), LogRecord ctor/dtor, error_code instantiation
│   ├── tracing.cpp                   # TraceProvider singleton + ScopedSpan ctors/dtor (uses OTel stub headers)
│   └── write_waves_to_elf.cpp        # writeWavesToElfMapped/Absolute
└── notes/
    ├── archive/                     # Historical / superseded content
    │   ├── OVERVIEW_phases_1-12.md  # 90KB detailed per-phase notes
    │   ├── TODO_phases_1-12.md      # 87KB completed task lists
    │   ├── unknowns_full_1-116.md   # Original numbered unknowns registry
    │   ├── todo_audit.md            # Phase 10.7a TODO marker audit (historical snapshot)
    │   └── phase_15b_prefetch_audit.md # Audit-only sub-phase wrap-up
    ├── unknowns.md                  # Active unknowns registry (Closed/Actionable/Blocked/Deferred)
    ├── struct_layouts.md            # Raw byte-offset tables for all structs
    ├── libcpp_abi.md                # libc++ vs libstdc++ ABI differences
    ├── pipeline.md                  # Compilation pipeline flow
    ├── opcode_map.md                # Full opcode table
    ├── opcode_encoding.md           # Instruction encoding formats (6 types)
    ├── optimization_passes.md       # AsmOptimize passes + register field semantics + algorithm reconstructions
    ├── node_tree_structure.md       # Node tree model
    ├── cervino_vs_hirzel.md         # Device family selection + difference matrix
    ├── device_type.md               # DeviceType / DeviceFamily / DeviceOption / DeviceOptionSet + sfc namespace + helpers
    ├── device_constants.md          # getDeviceConstants(AwgDeviceType) decode
    ├── awg_device_props.md          # AwgDeviceProps + AwgPathPatterns + getAwgDeviceProps<T> specializations
    ├── memory_allocator_analysis.md # MemoryAllocator detailed analysis
    ├── static_resources_cervino_consts.md # All 213 static constants
    ├── write_waves_to_elf.md        # writeWavesToElf analysis
    ├── elf_reader.md                # ElfReader layout corrections + ElfException + ELFIO API gotchas
    ├── logging_tracing.md           # zhinst::logging + zhinst::tracing reconstruction
    └── frontend_lowering.md         # EvalResults / EvalResultValue / Value layout / LowerResult / lower() pipeline
```

## Open Questions

Active unknowns are tracked in [`reconstructed/notes/unknowns.md`](reconstructed/notes/unknowns.md)
(Closed / Actionable / Blocked / Deferred sections).
Full pre-2026-04-22 history (items 1-116) archived in
[`reconstructed/notes/archive/unknowns_full_1-116.md`](reconstructed/notes/archive/unknowns_full_1-116.md).

Highest-value open items (Apr 2026):
- **#98**: FrontendLoweringState::result type — likely `shared_ptr<Node>`,
  promotion to "Closed" pending a second confirmation site
- **simplifyAssign** @0x280e10 and **splitReg** @0x281000 still have
  pre-correction register field usage (see `optimization_passes.md`)
- **reg0/reg1/reg2 naming** in assembler.hpp remains misleading —
  renaming deferred (cascading rename across 20+ files)
