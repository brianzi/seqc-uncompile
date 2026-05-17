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

**63 headers**, **99 source files**, **0 build errors**, **1 documented
placement-new warning** (libc++ vs libstdc++ string-size mismatch in
`value.cpp:237`, see `notes/libcpp_abi.md`).

Phases 1-41 complete plus the Phase D symbol-renaming audit and rename
execution (20 commits, all 16 high/medium-confidence clusters landed —
see `notes/symbol-renaming-audit/SYNTHESIS.md` and the per-commit
tables in
[`archive/OVERVIEW_symbol_renames_phases_DRS.md`](reconstructed/notes/archive/OVERVIEW_symbol_renames_phases_DRS.md)). 226
low-confidence/unsure cosmetic items remain explicitly deferred per audit
policy. **Phase R audit-followup complete** (14 commits
`dfc278e`..`2b23826`): all 6 deferred arbitrations and all 10 open
incidental findings (IF-110..IF-122) closed; 1 latent bug fixed
(IF-119, `setPRNGSeed` Var path), 1 missing writer reconstructed
(Arb 4, `Compiler::usedSampleRate_` mirror in `Compiler::compile`).
**Phase S Phase-Q refinement complete** (7 commits
`3f0b24e`..`04e3ac5`): the 226-item Phase Q backlog was triaged into
39 already-done, 58 formally wontfix, 14 mechanical-resolved (S.2
sweeps), and 114 borderline-deferred case-by-case items; no further
Phase Q phase planned. **95/95 undefined zhinst symbols resolved** —
static archive fully self-contained.

**Differential tests: 2407/2407 byte-identical** across all manifests
(current as of 2026-05-08, post-Phase-62 + Phase 44 audit + IF-200 fix):

| Manifest                                          | Cases |
| ------------------------------------------------- | -----:|
| main (default `manifest.json`) — imports the 6 leaf manifests below | 1601 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-core.json`      | 248 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-zhinst.json`    |  74 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-ziai.json`      | 459 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-ziasm.json`     | 468 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-zivibes.json`   | 259 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-documentation.json` |  93 |
| stress (`manifest-stress.json`, standalone)       | 774 |
| labone (`manifest-labone.json`, standalone)       |  14 |
| large (`manifest-large.json`, standalone)         |  13 |
| errors (`manifest-errors.json`, standalone)       |   6 |
| **Total unique cases**                            | **2407** |

Standalone manifests are not auto-imported and must be invoked
explicitly via `--manifest`.  Score has held at 2407/2407 across all
manifests since the Phase 44 audit + IF-200 fix on 2026-05-08.

### Reconstruction history (archived)

The detailed per-phase narrative for Phases 14a–25 (DeviceType,
ElfReader, csv_parser, custom_functions deep-dive, writeToNode,
AsmOptimize completion, SeqCAstNode evaluate(), magic-number
refactor, AST evaluate body expansion, pybind11 binding layer) has
been moved to
[`reconstructed/notes/archive/OVERVIEW_phases_13-25.md`](reconstructed/notes/archive/OVERVIEW_phases_13-25.md)
to keep this file focused on current-state reference.  All listed
phases are fully complete; the corresponding TODO entries are
archived under `reconstructed/notes/archive/` across three files:
[`TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md),
[`TODO_phases_13-39.md`](reconstructed/notes/archive/TODO_phases_13-39.md), and
[`TODO_phases_43-62.md`](reconstructed/notes/archive/TODO_phases_43-62.md).

For phase-by-phase context including reconstruction milestones
after pybind11 (Phases 30+, the differential-testing era and
beyond), see `TODO.md` Phase 44+ and `reconstructed/notes/incidental_findings.md`.


## Key Technical Findings

- **ABI**: Binary uses libc++ (string=24B, function=0x30, __tree=0x30). Build host uses libstdc++ (string=32B). Cannot static_assert sizeof for string-bearing structs. **Dual-build** validated: g++/libstdc++ (primary, `build/`) + clang++/libc++ (validation, `build-libcxx/`). See `notes/libcpp_abi.md`.
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
│   ├── assembler.hpp            # class Assembler (0x80), Assembler::Command enum (43 opcodes)
│   ├── play_config.hpp          # PlayConfig (0x20) — shift/mask constants
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
│   ├── device_type.hpp          # DeviceType base, DeviceTypeImpl, DeviceOptionSet, enums (Phase 14a)
│   ├── device_factories.hpp     # DeviceType factory: makeDefault, doMakeDeviceType (Phase 14b)
│   ├── device_subclasses.hpp    # 14 DeviceType family subclasses (Phase 14b)
│   ├── generic_device_type.hpp  # GenericDeviceType helper (Phase 14b)
│   ├── awg_device_props.hpp     # AwgDeviceProps (0xa0), AwgPathPatterns (0x60), getAwgDeviceProps<T>
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
│   ├── elf_reader.hpp           # ElfReader (0x98), ElfException, sectionAsString
│   ├── prefetch.hpp             # Prefetch (0x160), PrefetcherNodeState
│   ├── expression.hpp           # Expression (0x58), EOperationType/EOperator/ECommandType
│   ├── seqc_parser_context.hpp  # SeqcParserContext stub
│   ├── seqc_ast_node.hpp        # SeqCAstNode base (0x18) + 53 subclass declarations
│   ├── frontend_lowering.hpp    # FrontendLoweringContext (0x50), FrontendLoweringState (0x38)
│   ├── eval_results.hpp         # EvalResults (0x80), 14 methods
│   ├── eval_result_value.hpp    # EvalResultValue (0x38B struct)
│   ├── exception.hpp            # zhinst::Exception (0x60), ZIAWGCompilerException
│   ├── custom_functions.hpp     # CustomFunctions (0x1E0) + AccessMode + PlayArgs + NodeMap + exceptions
│   ├── math_compiler.hpp        # MathCompiler (23 single-arg + 5 multi-arg fns) + MathCompilerException
│   ├── node_map_data.hpp        # NodeMapData abstract base + Virt/DirectAddr subclasses + NodeMapItem (0x18)
│   ├── cached_parser.hpp        # CachedParser (0x60), CacheEntry, CachedFile
│   ├── waveform_generator.hpp   # WaveformGenerator (0xC0) + exception classes
│   ├── logging.hpp              # Severity (8 levels), ZiLogger tag, detail::LogRecord RAII
│   ├── tracing.hpp              # TraceProvider singleton, ScopedSpan RAII, getDefaultLabOneResource
│   ├── awg_compiler.hpp         # AWGCompiler pimpl (8B) + AWGCompilerImpl (0x2C0)
│   ├── zi_folder.hpp            # ZiFolder (24B) + DirectoryType enum
│   ├── calver.hpp               # CalVer (0x20 bytes: 4×size_t) + 12 free functions
│   ├── format_time.hpp          # formatTime (3 overloads)
│   ├── serial_predicates.hpp    # 10 isXxxSerial() device family predicates
│   └── platform.hpp             # getPlatformName()
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
│   ├── device_type.cpp               # DeviceType base + DeviceTypeImpl methods (Phase 14a)
│   ├── device_factories.cpp          # makeDefault, doMakeDeviceType factory (Phase 14b)
│   ├── device_hf2.cpp                # Hf2 family (Hf2li subtypes) (Phase 14b)
│   ├── device_mf.cpp                 # Mf family (Mfli/Mfia subtypes) (Phase 14b)
│   ├── device_uhf.cpp                # Uhf family (Phase 14b)
│   ├── device_hdawg.cpp              # Hdawg family (Phase 14b)
│   ├── device_shf.cpp                # Shf family (Shfqa/Shfsg/ShfqcSg/Shfli) (Phase 14b)
│   ├── device_pqsc.cpp               # Pqsc family (Phase 14b)
│   ├── device_hwmock.cpp             # Hwmock family (Phase 14b)
│   ├── device_shfacc.cpp             # Shfacc family (Phase 14b)
│   ├── device_ghf.cpp                # Ghf family (Phase 14b)
│   ├── device_qhub.cpp               # Qhub family (Phase 14b)
│   ├── device_vhf.cpp                # Vhf family (Phase 14b)
│   ├── device_unknown.cpp            # Unknown device fallback (Phase 14b)
│   ├── generic_device_type.cpp       # GenericDeviceType helper (Phase 14b)
│   ├── awg_device_props.cpp          # AwgDeviceProps + 9 getAwgDeviceProps specializations (Phase 14b-iii)
│   ├── mf_sfc.cpp                    # generateMfSfc (Phase 14e)
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
│   ├── compiler.cpp                  # Compiler pipeline: steps 1-19 all wired (Phase 30a-e)
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
│   ├── resources_function.cpp       # Function + scope methods (Batches 5-6)
│   ├── resources_static_global.cpp  # StaticResources/GlobalResources (Batch 7)
│   ├── exception.cpp                 # Exception ctors, dtor, what(), description()
│   ├── expression.cpp                # 20+ create* functions, str() for 3 enums
│   ├── seqc_ast_node.cpp             # 53 print/doClone bodies, str(EParamDirection), printSeqCAst
│   ├── frontend_lowering.cpp         # constWaveform stub
│   ├── custom_functions.cpp          # Core: ctor/dtor, call(), validators, PlayArgs (~1061 lines)
│   ├── custom_functions_play.cpp    # Play core: setWaitCyclesReg, play, playIndexed, writeToNode (~2462 lines)
│   ├── custom_functions_playback.cpp # Playback wrappers: playWave*, waitWave, sync, setRate (~994 lines)
│   ├── custom_functions_io.cpp      # I/O & config: DIO, wait, oscillator, QA, PRNG, feedback (~3456 lines)
│   ├── math_compiler.cpp             # MathCompiler: 23+5 wrappers + dispatch, fully complete (197 lines)
│   ├── node_map_data.cpp             # NodeMap hierarchy — fully reconstructed (Phase 17a)
│   ├── get_node_map.cpp              # GetNodeMap<> specializations — 8 device node maps, 1081 entries (Phase 26a)
│   ├── eval_result_value.cpp         # EvalResultValue dtor (extracted Phase 16b)
│   ├── cached_parser.cpp             # CacheEntry/File + 8 method stubs
│   ├── waveform_generator.cpp        # 2305 lines, all 32 DSP functions reconstructed, 2 exception classes
│   ├── logging.cpp                   # ZiLogger::construct_logger(), LogRecord ctor/dtor, error_code instantiation
│   ├── tracing.cpp                   # TraceProvider singleton + ScopedSpan ctors/dtor (uses OTel stub headers)
│   ├── write_waves_to_elf.cpp        # writeWavesToElfMapped/Absolute
│   ├── awg_compiler.cpp             # AWGCompilerImpl + AWGCompiler pimpl; 0 TODOs (all 8 resolved)
│   ├── calver.cpp                   # CalVer class: 16 symbols (ctor, accessors, pack/unpack, comparison)
│   ├── format_time.cpp              # formatTime: 3 overloads (ptime+fmt, ptime+bool, epoch+bool+bool)
│   ├── serial_predicates.cpp        # 10 isXxxSerial() range-check predicates
│   ├── platform.cpp                 # getPlatformName() → "linux64"
│   ├── zi_folder.cpp                # ZiFolder: folderPath, ziFolder, sessionSaveDirectoryName (Phase 24b)
│   ├── compile_seqc.cpp             # compileSeqc() orchestrator (Phase 24d)
│   └── pybind_seqc.cpp              # pybind11 entry points: pyCompileSeqc, makeSeqcCompiler (Phase 24e)
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
    ├── frontend_lowering.md         # EvalResults / EvalResultValue / Value layout / LowerResult / lower() pipeline
    ├── tools.md                     # Toolchain entry point (seqcc/seqas/seqdump) — cross-reference to tools/seqcc/DESIGN.md
    └── differential_testing.md      # Test approach, coverage matrix, future development ideas

tools/                              # Newly-written CLI toolchain over the reconstructed public APIs
├── seqcc/
│   ├── DESIGN.md                   # Authoritative design document for the driver
│   ├── CMakeLists.txt
│   ├── third_party/CLI11/          # Vendored CLI11 (header-only)
│   └── src/                        # See reconstructed/notes/tools.md §Source layout
```

## Toolchain (`seqcc` / `seqas` / `seqdump`)

The reconstructed compiler is reachable as a stand-alone command-line
driver (no Python required) since **Phase T**.  A single binary
`seqcc` provides gcc/clang-style staged compilation; two symlinks
`seqas` and `seqdump` dispatch on `argv[0]` to the legacy
`AWGAssembler` path and the ELF inspector respectively.

- Current version: `0.11.0-T10a` (`tools/seqcc/src/main.cpp`).
- Pipeline mapping: `--from=<stage>` / `--to=<stage>` flags mirror
  the public `step*` methods on `Compiler` (9 steps) and
  `AWGCompiler` (3 steps).  Full stage table in
  `reconstructed/notes/tools.md`.
- IR capture: since T10a the driver captures intermediate IR
  exclusively through the public accessors
  `AWGCompiler::compiler() → Compiler::{ast(), wavetable(), asmList()}`.
  The legacy `compileSeqcWithIR()` / `CompileSeqcIntrospection`
  introspection scaffold (IF-301) has been retired;
  `reconstructed/include/zhinst/codegen/compile_seqc.hpp` is back
  to the original-binary single-entry-point footprint.
- Source layout, build details, and the full design rationale live
  in `tools/seqcc/DESIGN.md`.  The shorter cross-reference entry
  point is `reconstructed/notes/tools.md`.
- Test gates: 70 tool-level cases under `tests/tools/`
  (`test_seqcc_smoke`, `test_seqcc_diff`, `test_seqcc_to`,
  `test_seqdump`) run independently of the 1612-case
  `diff_test_fast.py` ELF regression suite.  Both gates must stay
  green at every sub-phase wrap-up.

## Open Questions

Active unknowns are tracked in [`reconstructed/notes/unknowns.md`](reconstructed/notes/unknowns.md)
(Closed / Actionable / Blocked / Deferred sections).
Full pre-2026-04-22 history (items 1-116) archived in
[`reconstructed/notes/archive/unknowns_full_1-116.md`](reconstructed/notes/archive/unknowns_full_1-116.md).

Long-deferred items are listed in the **Deferred / Low Priority** section
of `TODO.md`; technical detail for the AsmOptimize trio (simplifyAssign,
splitReg, register-field rename) lives in `notes/optimization_passes.md`.

## Symbol Renames (Phases D, R, S) — archived

The Symbol-Renaming Audit (Phases D / R / S, 41 commits across
`d15ad32`..`04e3ac5`) produced the canonical names in
`reconstructed/include/zhinst/` today.  All three phases are
closed; per-commit tables and Phase-Q backlog disposition were
moved during D15 cleanup to
[`reconstructed/notes/archive/OVERVIEW_symbol_renames_phases_DRS.md`](reconstructed/notes/archive/OVERVIEW_symbol_renames_phases_DRS.md).
Source-of-truth analysis remains at
`reconstructed/notes/symbol-renaming-audit/SYNTHESIS.md`.


## Phase D — Inline code documentation

In progress.  See `TODO.md` for the phase breakdown (D0..D19) and
`reconstructed/docs/architecture.md` for the rendered Doxygen
mainpage.  Sub-phases **D0–D10 plus D-AUDIT-1/2/3 are complete**;
**D11–D19 are all complete**; Phase E (diff-test harness for
binding-unreachable reconstructions, including the 20 D16 symbols)
is the next open phase.  Current backlog tags (per `docs/coverage.sh`):
`\unclear=0`, `\verifyme=0`, `\binarynote=29`, `\unverifiable=5`.
Documentation coverage: 94.7% (2979/3145 symbols).  Strict-mode
doxygen warnings under
`WARN_IF_UNDOCUMENTED=YES`/`WARN_IF_DOC_ERROR=YES`/`WARN_NO_PARAMDOC=YES`/`WARN_IF_INCOMPLETE_DOC=YES`:
**0 binding warnings** as of `63cda64`.  Cleanup commits
`c662138`/`cd939f3`/`1aab213`/`a2584ee`/`63cda64` cleared 162 → 0;
the final commit applied the D11 `\cond INTERNAL` idiom (commit
2cd360b) to the 9 explicit specialisations of
`getAwgDeviceProps<T>` whose `\param`/`\return` blocks Doxygen
1.9.x cannot bind.

**D0 (setup) complete (2026-05-09):**
- Doxygen + Doxygen Awesome CSS configured under `reconstructed/docs/`.
- `cmake --build . --target docs` builds the site to
  `reconstructed/build/docs/html/`.
- Custom aliases `\unclear`, `\verifyme`, `\binarynote`,
  `\unverifiable` carry the accuracy-discipline metadata; each
  renders to a dedicated cross-reference page so the documentation
  backlog is discoverable.
- `reconstructed/docs/coverage.sh` reports current coverage from the
  generated XML.  Baseline at end of D0: 4/2712 symbols (0.1%).
- `reconstructed/notes/comment_style_guide.md` §13 added: documentation
  comments use `//!` and `/*! */`; reconstruction comments stay on `//`.

**D0 follow-ups (2026-05-10):**
- `reconstructed/docs/README.md` added — authoritative how-to for the
  doc system (markers, aliases, build, coverage, theme update, anti-
  patterns).  `AGENTS.md` carries a short pointer to it.
- Dark-mode contrast fix: `HTML_COLORSTYLE` switched from `LIGHT` to
  `AUTO_LIGHT`; `reconstructed/docs/theme/custom.css` added with
  `--code-foreground` / `--fragment-foreground` overrides under both
  the `prefers-color-scheme: dark` media query and the `html.dark-mode`
  class selector.  Vendor CSS stays a pristine pinned copy.

**D1 (architecture mainpage) complete (2026-05-10):**
- `reconstructed/docs/architecture.md` fleshed out: pipeline overview
  (12-step), per-subdirectory component map with `notes/` cross-
  references, top-level type-relationship diagram, ELF output section
  reference, accuracy-discipline statement, documentation roadmap.
- All content sourced from `OVERVIEW.md` and `notes/pipeline.md` —
  no new claims introduced.
- Coverage now reports 0/2712 (the prior 4/2712 figure was an artifact
  of `architecture.md` headings being counted as memberdef-equivalents
  before the page was extended; the current count reflects only true
  C++ symbol coverage, which D2 will start increasing).

**D2 (class-level briefs) complete (2026-05-10):**
- ~144 public classes briefed across 11 batches in topological order:
  `core/` (9), `infra/` (4), `io/` (5), `ast/` (24), `asm/` (12),
  `codegen/` (12 + 7 internals), `runtime/` (16), `waveform/` (18),
  `device/` (44).  `\binarynote` count: 4; `\unclear` / `\verifyme`
  counts: 0.
- Each batch followed a verify-then-write workflow: the brief was
  composed only after the corresponding `.cpp` (and, where relevant,
  the calling site) was inspected to confirm the claimed behaviour.
  Two cosmetic discrepancies surfaced in the process and were logged
  as IF-207 (`MESSAGE`/`ERROR_MSG` swap in an `asm_optimize.hpp`
  banner comment) and IF-208 (misleading `useDA` field name in
  `PrefetcherNodeState`); both promoted to D3 cleanup items.
- `WARN_IF_UNDOCUMENTED = YES` in `Doxyfile.in` from D2 wrap-up
  onward.  The 147 warnings in `build/docs/doxygen-warnings.log`
  are all parser cross-reference issues (overload mismatches on
  `evaluate(...)` overrides + macro-hidden factory dtors), not
  undocumented-symbol coverage gaps.  These are tracked under D3.
- 1600/1600 differential tests passing throughout the phase; commits
  span `4bff93b` through `4101782`.

**D3 (pipeline-driver functions) complete (2026-05-10):**

- 14 driver methods documented across 5 batches:
  - **Batch 1** (`d7685d2`): `Compiler::compile`, `Compiler::runPrefetcher`.
  - **Batch 2** (`ff1f747`): `Prefetch::{preparePlays, placeLoads,
    fillInPlaceholders}`.
  - **Batch 3** (`09dc245`): `AWGCompiler::writeToStream`,
    `AsmOptimize::{optimizePreWaveform, optimizePostWaveform}`.
  - **Batch 4** (`e80b126`): `WavetableIR::{updateWaveforms,
    assignWaveformAllocationSizes}`.
  - **Batch 5** (`0dffcc2`): `FrontEndLoweringFacade::lower`,
    `CustomFunctions::call`, `WaveformGenerator::{getOrCreateWaveform,
    call, eval}`.
- Each method documented with full `\brief` + `\details` (numbered
  pipeline steps) + `\param` + `\return` + `\throws`.
- IF-207 (MESSAGE / ERROR_MSG comment swap in `AsmOptimize`) and IF-208
  (`PrefetcherNodeState::useDA` → `crossesCacheLine` rename) closed in
  cleanup commit `0441b43`.
- **Doxygen warning cleanup** (`2cd360b`): 149 → 0 warnings.
  Strategy:
  - Enabled targeted macro expansion for the AST class-generator
    macros (`SEQC_TRIVIAL_LEAF`, `SEQC_UNARY`, `SEQC_OPERATOR`,
    `SEQC_BINARY`, `SEQC_LIST`) and the device-factory generator
    (`ZHINST_DECLARE_FACTORY`) by adding them to `EXPAND_AS_DEFINED`
    with `MACRO_EXPANSION=YES` + `EXPAND_ONLY_PREDEF=YES`.  Resolved
    ~140 warnings.
  - Wrapped explicit template instantiations in
    `src/core/error_messages.cpp` and `src/waveform/wave_index_tracker.cpp`
    in `\cond INTERNAL` (Doxygen cannot bind explicit-instantiation
    lines to their parameterised template declaration).
  - Wrapped the `BOOST_LOG_GLOBAL_LOGGER`-generated `ZiLogger` and
    `detail::LogRecord` definitions in `src/infra/logging.cpp` in
    `\cond INTERNAL`; public docs already live in the header.
  - Renamed `Waveform::File::operator==` to `WaveformFile::operator==`
    in `src/waveform/waveform.cpp` (the `using File = WaveformFile;`
    alias inside `class Waveform` was confusing Doxygen's symbol
    binder).
  - Re-aligned numbered-list markers in `compiler.hpp::compile` /
    `runPrefetcher` `\details` blocks so single- and double-digit
    items share the same column.
- 1600/1600 differential tests passing throughout the phase.

**D4 (per-class public-method briefs) complete:**

- D4 ordering: `Compiler` → `Prefetch` → `WavetableIR` /
  `FrontEndLoweringFacade` → `CustomFunctions` → `AsmOptimize` →
  `WaveformGenerator` → `Resources` → AST nodes.  Per-class commits;
  large classes are subdivided into sub-batches.
- **Batch 1 — `Compiler`** complete (`381c1ee`).  Surfaced and fixed
  IF-209 (constructor missing `nodeFactory_` initialisation) and
  IF-210 (`runPrefetcher` ABI signature drift), both GDB-confirmed
  before the fix (`01f4bd9`).
- **Batch 2 — `Prefetch`** complete across five sub-batches:
  - **2a** (`abde6a6`): constructor / destructor / 3 prepare-pass
    helpers.  Surfaced IF-212 (cosmetic comment drift, fixed in
    same commit) and IF-213 (`findLockedPlay` stub, GDB-confirmed
    real divergence; deferred).
  - **2b** (`0c1e524`): 4 placement-precondition methods.  IF-214
    + IF-215 fixed in cleanup (`31f0cbc`).
  - **2c** (`b025126`, `d247227`): 4 cache / placement-helper
    methods.  IF-216 fixed; IF-213 GDB-confirmed.
  - **2d** (`1e7cd32`): 13 tree-rewrite helpers.  Surfaced IF-217
    (`backwardTree` `next` vs `loop`), IF-218 (`expandSetVar`
    stub), IF-219 (`createLoad` missing already-loaded guard) —
    all deferred — and IF-220 (cosmetic comment cluster, fixed in
    same commit).  TODO entries with full GDB recipes filed
    (`e93aea3`).
  - **2e-i** (`3a7ed28`): `placeCommands`, `findPlaceholder`,
    `placeSingleCommand`.  IF-221 (cosmetic, fixed: `Branch`
    nodes mislabelled as "placeholder" in skip-loop), IF-222
    (cosmetic, fixed: file-header NodeType labels for Table /
    PlainLoad / AwgReady), IF-223 (Table case missing
    smap/ssl/addr/prf tail; deferred), IF-224
    (`play_cervino_indexed_nonsplit` empty body; deferred).
  - **2e-ii** (`cb4f1e6`): 7 waveform-instruction helpers
    (`clampToCache`, `wvfImpl`, `wvfRegImpl`, `wvfs`,
    `needsNewCwvf`, `splitPlay`, `insertPlay`).  No new IFs.
  - **2e-iii** (`33a40ce`): 4 cache / query / debug-printer
    methods (`getUsedCache`, `getUsedChannels`,
    `getUsedFourChannelMode`, `print`).  IF-225 (cosmetic, fixed:
    `getUsedChannels` block-header named the reduced field
    `channelMask` when body and binary read `suppress`), IF-226
    (`getUsedCache` body is a stub returning 0; deferred — only
    caller is the debug-only `print`, so no production effect).
- **Open IFs from D4** (all with full TODO entries and
  objdump / GDB recipes): IF-213, IF-217, IF-218, IF-219, IF-223,
  IF-224, IF-226, IF-228 (cosmetic).  IF-230 / IF-231 / IF-232 /
  IF-234 (D-AUDIT-1 family) all closed; D-AUDIT-1 itself
  truly closed at sweep end (2026-05-10).  Each remaining
  open IF is latent — the differential test corpus does not
  exercise the divergent code path, hence 1602/1602 remains
  green.
- **Batch 3 — `WavetableIR` / `WavetableFront` /
  `WavetableManager<T>` + frontend-lowering structs** complete
  across five sub-batches (59 briefs total + 1 latent bug fix):
  - **3a** (`b5a38cc`): `detail::getUniqueName` plus 7
    `WavetableIR` accessors / iterators.  Surfaced and fixed
    IF-227 in the same commit (`WavetableIR::size()` recon body
    divided element count by `sizeof(shared_ptr)` = 16, silently
    yielding `count/16`; binary at `0x29e290` performs the
    divide at the raw libc++ pointer level; replaced with
    `manager_->waveforms_.size()`).  Latent — no live caller.
  - **3b** (`a375672`): 12 `WavetableIR` ctor / dtor /
    serialization / allocation briefs.  Combined with the D3
    briefs for `updateWaveforms` and
    `assignWaveformAllocationSizes`, `WavetableIR` is now
    fully documented.
  - **3c-i** (`445c0c0`): 10 `WavetableFront` lifecycle and
    simple-accessor briefs (ctor, dtor, `dummyWarning`,
    `begin`, `end`, `setWarningCallback`, `getMemorySize`,
    `toString`, `loadWaveform`, `setLineNr`).
  - **3c-ii** (`91d9e41`): 13 `WavetableFront` factory and
    query/utility briefs.  `WavetableFront` now fully
    documented.
   - **3c-iii** (`f08c46f`): 14 `WavetableManager<T>` templated
     method briefs (covering both `WaveformFront` and
     `WaveformIR` instantiations) plus the two trivial
     frontend-lowering dtors (`FrontendLoweringContext::~`,
     `FrontendLoweringState::~`).
- **Batch 4 — `CustomFunctions`** complete across eight
  sub-batches (130+ briefs total):
  - **4a** (`e8a2600`): periphery — exception classes, lookup
    helpers, `AccessMode`, `PlayArgs`, `NodeMap`.
  - **4b** (`03ebeea`): dispatch surface — `call`,
    `executeCall`, registration plumbing.
  - **4c** (`a3c6af5`): `custom_functions_play.cpp`
    play-family built-ins.
  - **4d** (`0f91745`): `custom_functions_waveforms.cpp`
    waveform built-ins.
  - **4e** (`3ebf2e3`): `custom_functions_oscillators.cpp`
    oscillator / phase / DIO built-ins.
  - **4f** (`bafa9a9`): `custom_functions_qa_feedback.cpp`
    QA / feedback built-ins.  Corrected the AwgDeviceType
    bit-decomposition table during the pass (no `UHFAWG`
    enumerator exists; `kDevHirzel = 0x1F2`, etc.).
  - **4g** (`fbc7f4a`): `custom_functions_wait_trigger.cpp`
    wait / trigger / sine-phase built-ins (16 methods).
  - **4h** (`5821661`): `custom_functions_registers.cpp`
    register / trigger / QA / sweep built-ins (31 methods).
  Header has zero undocumented `EvalResults`-returning
  declarations after the run; 0 doxygen warnings at every
  commit; 1600/1600 maintained throughout.
- **Batch 5 — `AsmOptimize`** complete (`25f5aa0`).  Audited
  the five existing briefs (`OptimizeException`, the class
  brief, `optimizePreWaveform`, `optimizePostWaveform`); all
  accurate against the binary, no IF needed.  Added briefs
  for the three undocumented public lifecycle members (ctor,
  dtor, `prepareResources`) and for **all 17 private members**
  — 5 query helpers (`isRead`, `isWritten`, `isLabelCalled`,
  `getNextActionForReg`, `registerIsNeverWritten`), 8
  peephole passes (`deadCodeElimination`,
  `oneStepJumpElimination`, `removeUnusedLabels`,
  `mergeLabels`, `mergeRegisterZeroing`, `removeUnusedRegs`,
  `reportUserMessages`, `simplifyAssign`), and 4
  register-allocator routines (`registerAllocation`,
  `splitConstRegisters`, `splitReg`, `registerUpdate`).
   Flipped `EXTRACT_PRIVATE` to `YES` in `Doxyfile.in` so the
   new private briefs surface on the class page; the global
   warning count remains at 0 because `EXTRACT_ALL=NO` keeps
   undocumented privates silently skipped elsewhere in the
   tree.  Corrected one stale legacy reconstruction comment
   on `isLabelCalled` (the scan range is `[begin, it)`, not
   "after `it`") in the same commit.
- **Batch 6 — `WaveformGenerator`** in progress, sub-batched
  into 6a/6b/6c/6d.  6a/6b/6c/6d complete:
  - **6a** (`389b48e`): lifecycle / readers / shape helpers
    (~14 briefs).  IF-229 fixed (cosmetic alias-substitution
    overstatement on the class brief).
  - **6b** (commit `bf89075` for fixes; `a861b55` for briefs):
    18 simple-shape factory briefs (`zeros`, `ones`, `sin`,
    `cos`, `sinc`, `ramp`, `sawtooth`, `triangle`, `gauss`,
    `drag`, `blackman`, `hamming`, `hann`, `rect`, `mask`,
    `marker`, `rrc`, `vect`).  Surfaced and fixed IF-230 in
    `rrc` 3-arg path and `sinc` 4-arg path
    (arity-blind hardcoded parameter-label strings); each
    `\binarynote` cross-references IF-230.  No LaTeX in briefs
    — formulas use plain ASCII in backticks because the doc
    build has no LaTeX.
  - **6c** (commits `73a2e5f`, `d68f5a3`, `83f4973`): 8 briefs
    for `chirp`, `rand`, `randomGauss`, `randomUniform`,
    `lfsrGaloisMarker`, `placeholder`, `filter`, `circshift`.
    Audit surfaced two new IFs:
    - **IF-231** (`73a2e5f`): `rand` 3-arg semantic bug.  The
      recon implemented `rand(length, amplitude, mean)` with
      `stddev` defaulting to 1.0; GDB-tracing the binary on
      `rand(64, 0.25, 0.125)` proved the actual signature is
      `rand(length, mean, stddev)` with `amplitude` defaulting
      to 1.0 — exact symmetry with IF-205 for `randomGauss`.
      Fixed and added the first 3-arg `rand` test case
      (`hdawg_doc_random_waves_3arg`, +1 to suite total).
    - **IF-230 extension** (`d68f5a3`): `chirp` had 7 labels
      using camelCase that differed from the binary's spaced
      strings (`startFrequency` → `start frequency`, etc.);
      `lfsrGaloisMarker`'s second label was `"2 (marker)"`
      instead of the binary's `"2 (markerBit)"`.  All
      GDB-verified via SSO inline-byte inspection.
    The briefs commit (`83f4973`) carries `\binarynote`
    cross-references on `rand` and `randomGauss` for IF-205 /
    IF-231.  D-AUDIT-1 in `TODO.md` updated with the
    per-factory progress checklist.
  - **6d**: 9 briefs for the `WaveformGenerator` combinators
    in `waveform_generator_dsp.cpp` — `join`, `add`,
    `interleave`, `scale`, `multiply`, `cut`, `flip`, `merge`,
    `grow`.  All briefs verify-then-write against the function
    bodies (not the legacy block-header summaries); existing
    incidental findings (IF-162 length accumulation in `merge`,
    IF-176 `cut(from==from)` channels=0 propagation, IF-181
    `join` reserve-only materialisation, IF-188 `add` no
    reserve-only short-circuit) cited inline so the user-facing
    docs surface the documented quirks.  No new IFs.
    Build clean (0 doc warnings); tests 1601/1601.
  - **7c (2026-05-10)**: 34 briefs in
    `include/zhinst/codegen/math_compiler.hpp` — full
    `MathCompiler` public surface (ctor + 23 unary + 5
    variadic + `functionExists` + `call`) and the three
    `MathCompilerException` members.  Documents the
    `ln`/`log` aliasing (both natural log; use `log10` for
    base-10), `sign` returning `0.0` for NaN, `avg([]) →
    NaN`, `max/min` UB on empty input, the `pow` arity
    self-check, the `strict` flag's lazy-evaluation
    semantics in `functionExists`, and `call`'s asymmetric
    diagnostics (`UnknownFunction` / `FuncSingleArg`,
    variadic arity NOT validated except by `pow`'s own
    self-check).  No new IFs.  Build clean (0 doc
    warnings); tests 1601/1601.
  - **D-AUDIT-1 closeout (2026-05-10)**: completed the
    parameter-label sweep across the remaining 11
    `WaveformGenerator` factories (`gauss`, `sin`, `cos`,
    `sawtooth`, `triangle`, `drag`, `blackman`, `hamming`,
    `hann`, `placeholder`, `vect`).  Only `gauss` (0x24ddb0)
    diverged: 4-arg labels were unindexed (`"length"` →
    `"1 (length)"`, …), 3-arg labels likewise, the standard
    deviation was named `"width"` instead of the binary's
    `"sigma"`, and both arities called `readInt` where the
    binary calls `readPositiveInt`.  Fixed in
    `waveform_generator_dsp.cpp` and the gauss header brief
    (logged as IF-232).  All other 10 factories
    (`vect` excluded — dynamic labels) match the binary
    exactly; D-AUDIT-1 closed.  Tests 1601/1601.
  - **6e (2026-05-10)**: completed `WaveformGenerator` header
    coverage by briefing the two exception-class ctors/dtors
    (`WaveformGeneratorException`,
    `WaveformGeneratorValueException`) and the
    `argIndex()` accessor — the only remaining undocumented
    public symbols on the class surface.  All ~50 public
    methods, factories, and helpers in
    `include/zhinst/waveform/waveform_generator.hpp` now
    carry full briefs.  Build clean (0 doc warnings); tests
    1601/1601.
  - **7a (2026-05-10)**: 11 briefs for the public-facing
    `AWGCompiler` facade in
    `include/zhinst/codegen/awg_compiler.hpp` — the topmost
    C++ API the Python `_seqc_compiler.compile_seqc` binding
    wraps.  Covered: ctor, dtor, `compileString`, `compileFile`,
    `addWaveforms`, `writeToFile`, `writeAssemblerToFile`,
    `getCompileReport`, `getJsonWaveformMemoryInfo`, both
    `set*Callback` setters.  Each brief is verify-then-write
    against the corresponding `AWGCompilerImpl::*` body in
    `awg_compiler.cpp` (the facade methods themselves are
    one-line pimpl forwards), so the documentation describes
    actual behaviour rather than the trivial dispatch.
    `compileString`'s brief cites IF-192 / IF-195 for the
    historical `maxSequenceLen`-vs-`maxProgramSize` field
    mis-binding.  No new IFs.  Build clean (0 doc warnings);
    tests 1601/1601.
  - **7b (2026-05-10)**: 12 briefs for `CompilerMessage`,
    `CompilerMessageCollection`, and `CompilerException` in
    `include/zhinst/core/compiler_message.hpp` — the
    diagnostic-message infrastructure used by every compile
    pass.  Verify-then-write against the impls in
    `src/core/compiler_message.cpp` surfaced one
    documentation-correctness IF (IF-233): the recon `.cpp`
    correctly omits the `hadError_` write in `parserMessage`,
    matching the binary's 5-instruction tail-call body, but a
    stale narrative comment in `compiler.cpp:303-308` claimed
    the opposite.  Verified the asymmetry against the binary
    via `objdump` of `errorMessage` (writes
    `movb $0x1,0x1c(%rbx)` after the `compilerMessage` call)
    and `parserMessage` (no such write); fixed the stale
    comment to describe the actual invariant (parse-error
    short-circuit lives on `SeqcParserContext::hadSyntaxError_`,
    not `messages_.hadError_`) and added a `\binarynote` to
    the `parserMessage` brief cross-referencing IF-233.
    Build clean (0 doc warnings); tests 1601/1601.
  - **7d (2026-05-10)**: 4 briefs for `AWGCompilerConfig` in
    `include/zhinst/codegen/awg_compiler_config.hpp` —
    destructor plus the three method/static helpers
    (`getAwgDeviceTypeString`, `getAwgDeviceTypeFromString`,
    `getChannelGroupingModeString`).  Documents the full
    bit-flag → marketing-string table (1=UHFLI..256=VHFLI),
    the inverse table keyed by the internal Zurich
    Instruments project codenames (cervino / hirzel /
    klausen / grimsel_* / gurnigel / maloja —
    case-insensitive `boost::iequals`), and the HDAWG-only
    channel-grouping mode strings (`"4x2"`, `"2x4"`, `"1x8"`).
    Codename → enum-value mapping spot-checked against
    `objdump` of `getAwgDeviceTypeFromString` at 0x270180
    (verified `cervino → 1`, `hirzel → 2`).  No new IFs.
    Build clean (0 doc warnings); tests 1601/1601.
  - **7c (2026-05-10)**: 34 briefs in
    `include/zhinst/codegen/math_compiler.hpp` — full
    `MathCompiler` public surface (ctor + 23 unary + 5
    variadic + `functionExists` + `call`) and the three
    `MathCompilerException` members.  Documents the
    `ln`/`log` aliasing (both natural log; use `log10` for
    base-10), `sign` returning `0.0` for NaN, `avg([]) →
    NaN`, `max/min` UB on empty input, the `pow` arity
    self-check, the `strict` flag's lazy-evaluation
    semantics in `functionExists`, and `call`'s asymmetric
    diagnostics (`UnknownFunction` / `FuncSingleArg`,
    variadic arity NOT validated except by `pow`'s own
    self-check).  No new IFs.  Build clean (0 doc
    warnings); tests 1601/1601.
  - **7e (2026-05-10)**: 21 briefs across three small
    zero-coverage utility headers — `core/platform.hpp`
    (10 free functions: `getPlatformName`,
    `isDirectoryWriteable`, `isMountPoint`, `isPureAscii`,
    two `isValidUtf8` overloads, `isInList`, `quote`, two
    `toSubscript` overloads, `toSuperscript`,
    `escapeStringForMatlab`), `core/format_time.hpp`
    (3 `formatTime` overloads), `device/serial_predicates.hpp`
    (10 `is*Serial` predicates documenting the per-family
    short/long serial-number ranges).  Two `\binarynote`
    annotations: `isDirectoryWriteable`'s non-unique
    `Info.txt` probe filename, and `toSubscript(long)`
    silently dropping negative signs.  No source changes;
    no new IFs.  Build clean (0 doc warnings); tests
    1601/1601.
  - **7f (2026-05-10)**: ~80 briefs covering the full
    `AsmCommands` public surface in
    `include/zhinst/asm/asm_commands.hpp` — the
    instruction-builder façade.  Documents the constructor,
    every assembler mnemonic emitter (waveform playback /
    branch / ALU register-register / shift / ALU
    immediate-unsigned and signed / `Value`-overloads /
    load-store / trigger / misc I/O / control flow / sync /
    pseudo-instructions / node-creating commands /
    placeholder management / `genPlayConfig` / `asmPlay` /
    `asmTable` / misc), plus `setWavetableFrontIndex` and
    `wavetableFrontIndex` accessors.  Calls out: the
    multi-instruction expansion rules in `alui` (and
    `addi32`'s always-2-instruction variant), the
    `Value`-overload error path through `toInt32` /
    `errorHandler_`, the `luser`/`suser` aliasing of
    `ld`/`st`, the dummy-play path through `asmPlay` with
    empty `waveforms`, and the marker-processing gating on
    `impl_->isCWVFRSupported()` (UHF/Cervino devices skip
    marker bits).  One `\binarynote` on `genPlayConfig`
    flagging the duplicated four-channel boolean
    parameters.  Pre-existing `// ====` block comments and
    field-offset annotations preserved verbatim alongside
    the new `//!` briefs.  No source changes; no new IFs.
    Build clean (0 doc warnings); tests 1601/1601.
  - **7g (2026-05-10)**: ~40 briefs across the
    `AsmList` / `AsmList::Asm` public surface in
    `include/zhinst/asm/asm_list.hpp`.  Documents the
    `Asm` member set (`lineNumber` dual-purpose
    accessor, `~Asm`, `serializeNodeToJsonString`,
    `createUniqueID` TLS counter, identity-only
    `operator==`), the `AsmList` lifecycle (3 ctors,
    dtor, `append`), the round-trip text I/O group
    (`print`, `serialize` two-pass algorithm,
    `deserialize`, `parseStringToAsmList` with the
    HDAWG-hardcoded `\binarynote`), `maxRegister`,
    every container-forwarding member (iterators /
    `size` / `empty` / `reserve` / `push_back` /
    `insert` / `erase` / `clear` / indexed access /
    `front`/`back` / `operator==` / ADL `swap`), the
    two `insert(placeholder, …)` overloads (with the
    "append on miss" fallback documented), the four
    implicit single-`Asm` ctor / assignment forms used
    by the prefetch pass, and the `nextSequenceId()`
    free-function alias.  No source changes; no new
    IFs.  Build clean (0 doc warnings); tests
    1601/1601.
  - **7h (2026-05-10)**: ~40 briefs across
    `include/zhinst/device/device_type.hpp` covering the
    four core enums (`DeviceFamily` one-hot bitmask,
    `DeviceTypeCode`, `DeviceOption`, all six
    `sfc::*Option` per-family masks), the
    `DeviceOptionSetConstIterator` / `DeviceOptionSet` /
    `detail::DeviceTypeImpl` / `DeviceType` public
    member surfaces, the `OptionCodePair` /
    `initializeSfcOptions` / `generateMfSfc` template
    helpers, and every free string-conversion / parsing
    / predicate function (`toString` overloads, `expand`,
    `toDeviceTypeCode`, `toDeviceFamily`,
    `toDeviceOption`, `toDeviceOptions`,
    `splitDeviceOptions`, `is*` predicates, `hasMf`,
    `allDevices`).  Three `\binarynote` annotations
    capture the family-context oddities documented in
    notes: `DeviceOption(0)`/`(6)` rendering as
    `MFK`/`RTK` only on HF2, `toString(DeviceType)`
    intentionally omitting options, `hasMf` swapping
    `MF`/`MD` based on family, and `toDeviceFamily`'s
    special tokens (`"none"`, `"DEFAULT"`, `SHFACC`,
    `SHFPPC2`, `SHFPPC4`).  No source changes; no new
    IFs.  Build clean (0 doc warnings); tests
    1601/1601.
  - **7i (2026-05-10)**: Briefs across
    `include/zhinst/core/types.hpp`: `AwgDeviceType`
    one-hot enum + per-value briefs, the 12 `kDev*`
    named bitmask combinations, the `EDirection` enum
    + per-value briefs, the four `kRateInherit` /
    `kNoWaveIndex` / `kNoNodeId` / `kNoPlayIndex`
    sentinels, the two `kChannelTag_I` / `kChannelTag_Q`
    `writeToNode` channel tags, and every SUSER
    register-address constant grouped under a single
    `\name` Doxygen group (write-protocol, trigger /
    timing, sync, AWG-core / wait, oscillator / sine
    phase, PRNG, QA, frequency-sweep, misc) plus the
    two LD/ST direct-address constants in their own
    group.  Cross-references the
    `cervino_vs_hirzel.md` and `special_registers.md`
    notes for the full register map.  Existing
    block-header comments preserved verbatim alongside
    the new `//!`/`//!<` briefs.  No source changes;
    no new IFs.  Build clean (0 doc warnings); tests
    1601/1601.
  - **7j (2026-05-10)**: ~40 briefs across
    `include/zhinst/ast/expression.hpp`: the three
    parser-AST tag enums (`EOperationType`,
    `EOperator`, `ECommandType`) with per-value briefs
    rendering the source-token correspondence; the
    three `str()` debug-printer overloads; every
    `create*` parser-action factory (`createValue`,
    `createString`, `createVariable`,
    `addVariableType`, `createVariableType`,
    `createOperator`, `createAssignOperator`,
    `createArray`, `createListType` +
    `createOrAppend{ListType,ArgList,DeclList,
    ParamList,StmtList}`, `createFunctionCall`,
    `createFunction`, `createCommand` variadic, and
    every control-flow `createIf` /
    `createIfElse` / `createSwitch` / `createCase` /
    `createCondExpression` / `createFor` /
    `createWhile` / `createRepeat` / `createDoWhile`);
    plus `copyExpression` and the bison `seqc_error`
    callback.  `seqc_error` brief verified against
    `expression.cpp:583` — return value is 1 (not 0)
    and the implementation calls
    `ctx->raiseError(msg)` followed by
    `ctx->setSyntaxError()`.  No source changes; no
    new IFs.  Build clean (0 doc warnings); tests
    1601/1601.
  - **7k–7o (2026-05-10)**: AST-subsystem brief sweep
    across five headers: `ast/value.hpp` (Immediate
    variant + ValueException + Value with 4 ctors,
    toDouble/toInt/toBool/toString/operator==), 
    `ast/eval_results.hpp` + `ast/eval_result_value.hpp`
    (EvalResults with 9 setValue overloads, getValue
    last-element semantics, addAssembler), 
    `ast/seqc_parser_context.hpp` (parse-state machine
    incl. comment-state edges and the
    `hadSyntaxError_`-vs-`messages_.hadError_`
    asymmetry from IF-233), `ast/node.hpp` (Node base,
    20-param ctor, swap throwing
    `ZIAWGCompilerException(SwapNotConnected, 0xa4)`
    when `b->parent` doesn't lock to `a`,
    toJson/installPointers two-pass serialization), and
    `ast/seqc_ast_node.hpp` (base virtuals + leaf /
    unary / operator / binary / list family
    class-level briefs — Doxygen can't apply per-method
    briefs inside the macro expansions, so coverage is
    by class).  No source changes; no new IFs.  Each
    sub-batch verified body-then-brief; surfaced no
    documentation-correctness drift in this pass.
  - **7k-fix–7o-fix (2026-05-10)**: backfill
    `\param`/`\return` blocks across the same five AST
    headers to satisfy `WARN_NO_PARAMDOC=YES`.  Doc
    warnings dropped 459 → 396 (-63) across the five
    fixes.  The `seqc_ast_node.hpp` base-virtual fix
    cascades onto every concrete derived node — Doxygen
    treats inherited briefs as documenting overrides
    too, so silencing the four base virtuals
    (`evaluate`/`getListElements`/`children`/`print`)
    silenced inherited-comment warnings on every
    `SeqC*Node` subclass.  Build clean; tests 1601/1601.
  - **7p (2026-05-10)**: `\param`/`\return` backfill on
    `include/zhinst/asm/asm_commands.hpp` — all 73
    instruction emitter methods covering waveform,
    branch, ALU, I/O, trigger, table-mapping, prefetch,
    sync, and placeholder families.  Subagent-dispatched
    + independently verified.  Doc warnings 315 → 167
    (-148; file fully clean).  Build clean; tests
    1601/1601.
  - **7q (2026-05-10)**: `\param`/`\return` backfill on
    `include/zhinst/device/device_type.hpp` — covers
    `DeviceOptionSet` iterator/container,
    `detail::DeviceTypeImpl` hierarchy, and the public
    `DeviceType` facade.  Adding `\return` to the base
    `DeviceTypeImpl::clone()` collaterally silenced 33
    inherited-comment warnings on every subclass override
    in `device_subclasses.hpp`.  Doc warnings 167 → 94
    (-73).  Build clean; tests 1601/1601.
  - **7r (2026-05-10)**: `\param`/`\return` backfill on
    `include/zhinst/codegen/math_compiler.hpp` — 23
    unary math functions (`abs`, full trig +
    inverse + hyperbolic pairs, `exp`/`ln`/`log`/`log2`/
    `log10`, `sign`, `sqrt`, `ceil`/`round`/`floor`)
    plus the 4 variadic helpers (`max`, `min`, `pow`,
    `sum`).  Pre-existing `\throws` on `pow` preserved.
    Doc warnings 94 → 40 (-54).  Build clean; tests
    1601/1601.
  - **7s (2026-05-10)**: `\param`/`\return` backfill on
    `include/zhinst/asm/asm_list.hpp` — 23 declarations
    covering `Asm::operator==`, the `AsmList` container
    surface (ctors, assignment, iterator family,
    capacity, mutators, element access, comparison), and
    the placeholder-targeted `insert` overloads.  Doc
    warnings 40 → 0 (-40).  **Entire reconstructed
    codebase now reports zero Doxygen warnings under
    strict `WARN_IF_UNDOCUMENTED=YES`,
    `WARN_IF_DOC_ERROR=YES`, `WARN_NO_PARAMDOC=YES`.**
    Build clean; tests 1601/1601.  Documentation
    coverage at sweep close: 874/3081 symbols (28.4%);
    backlog: 1 `\unclear`, 0 `\verifyme`, 43
    `\binarynote`.
  - **D-AUDIT-1 closeout for real (2026-05-10)**: the
    previous "closeout" after `gauss` was premature —
    11 multi-arity factories had not actually been
    audited.  Re-audited the remaining 11
    (`gauss`, `sin`, `cos`, `sawtooth`, `triangle`,
    `drag`, `blackman`, `hamming`, `hann`, `vect`,
    `placeholder`).  Six clean (`drag`, `blackman`,
    `hamming`, `hann`, `vect`, `placeholder`).
    `gauss` already fixed under IF-232.  The four
    trig-family factories (`sin`/`cos`/`sawtooth`/
    `triangle`) had a *semantic* 3-arg parameter-binding
    bug — same shape as IF-205 (`randomGauss`) and
    IF-231 (`rand`): bound `(length, amplitude, phase)`
    but binary binds `(length, phase, nPeriods)` with
    `amplitude=1.0`.  Promoted to **IF-234** and fixed
    in the same edit, plus 8 cosmetic 4-arg label drifts
    (`"3 (phase)"` → `"3 (phase offset)"`,
    `"4 (nPeriods)"` → `"4 (number of periods)"`).
    Coverage test `hdawg_doc_trig_3arg.seqc`
    (manifest entry `trig_3arg`) added — exercises both
    arities of all four factories; byte-identical to
    binary post-fix.  Audit-method clarifications
    (objdump end-address derivation, `movupd` vs
    `movups`, rip-rel hex prefix, `objdump -s`
    byte-group cap) folded back into TODO.md
    D-AUDIT-1 recipe and IF-230 "Likely scope".
    All 16 multi-arity factories now audited;
    D-AUDIT-1 truly closed.  Tests 1602/1602
    (added `trig_3arg`); 0 doc warnings.
- **Verify-then-write workflow** (AGENTS.md §"Verify-then-write")
  formalised during 2d-2e: every brief opened the function body
  and cross-checked field names against the canonical `.hpp`
  before being written; subagent audits were re-verified before
  IFs were logged.  This caught one false-positive audit claim
  (the audit reported `PlayConfig::now` as misnamed; verification
  showed `now` is the canonical field per `play_config.hpp:47`,
  so no IF was logged).
- 1600/1600 → 1601/1601 → 1602/1602 differential tests
  passing throughout the phase (incremented as new coverage
  tests were added: `random_waves_3arg` for IF-231,
  `trig_3arg` for IF-234); build clean and 0 doxygen
  warnings at every commit.

**D5 (internal helpers / opcodes / leaves) complete:**

- Coverage went from 28.4% (start of D5) to **95.2%** (2932/3079
  symbols) across batches D5-1..D5-20.  Remaining 41 zhinst::
  undocumented entries are 35 TU-local anonymous namespaces with
  Doxygen-mangled IDs (not user-meaningful) plus 6 stragglers —
  judged not worth further pursuit.
- Class-member gap eliminated: 1048 → 0 across 161 classes
  (D5-11..D5-17 cumulative).  zhinst:: free-function gap also
  eliminated end of D5-10.
- §14 of `notes/comment_style_guide.md` codified during D5-AUDIT
  (IF-238): documentation passes preserve **content** (binary
  addresses, field offsets, ABI padding comments, JSON-key
  inventories, notes/ cross-refs) when adding `//!` briefs;
  hex-token preservation audits enforce this on subagent dispatch.
- §15 added at D5-20 wrap-up: macro-body documentation rules
  covering three pitfalls — `//!` continuation hazard inside
  backslash-line-continued macro bodies, `MACRO_EXPANSION` brief
  mis-attachment, per-invocation brief plus `\name` group for
  macro-defined symbol families.
- New IFs filed during the phase: IF-235..IF-240.
- 1602/1602 differential tests passing throughout; 0 doc
  warnings at every commit.

**D6-A (notes/ pages — promote evergreen as-is) complete:**

- 39 notes-files audited and classified: 13 EVERGREEN (promote
  as-is, this batch), 16 MIXED (D6-B, deferred), 10 WORKING-DOC
  (excluded — `incidental_findings.md`, `unknowns.md`, dated
  audit logs).
- 13 evergreen notes promoted via per-file include list in
  `reconstructed/CMakeLists.txt` (the `DOXYGEN_NOTES_PAGES`
  variable), each carrying a `{#notes_X}` anchor and a
  one-paragraph "reverse-engineering reference material" banner.
  Voice-policy carve-out approved: these pages cite binary
  addresses inline (the standard `\binarynote`-only rule applies
  to API doc comments, not to RE reference pages).
- Pages: `notes_pipeline`, `notes_cervino_vs_hirzel`,
  `notes_opcode_encoding`, `notes_opcode_map`,
  `notes_fb_instruction`, `notes_goto_policy`,
  `notes_special_registers`, `notes_node_tree_structure`,
  `notes_static_resources_cervino_consts`,
  `notes_waveform_generator_funcmap`,
  `notes_memory_allocator_analysis`, `notes_logging_tracing`,
  `notes_elf_format`.
- `architecture.md` cross-links them under a new
  "Reverse-engineering reference" section and updates the stale
  "Documentation roadmap" section (was "D2 wrap-up"; now reflects
  D5-closed / D6-A complete / D6-B complete).
- 1602/1602 tests passing; 0 doxygen warnings; build clean.

**D6-B (promote 14 of 16 MIXED notes/ files) complete:**

- 14 MIXED notes promoted across 3 commits (`64a1601` batch 1 of 5,
  `a837454` batch 2 of 7, `de1f165` batch 3 of 2).  Each gained a
  `{#anchor}` title and the standard reverse-engineering banner;
  phase tags, dated audit snapshots, RESOLVED markers, "Open items
  / Open questions" lists, "Recon bugs" trailers, and similar
  transient framing were stripped.
- New pages: `notes_awg_device_props`, `notes_binary_contents_excluded`,
  `notes_device_constants`, `notes_seqc_language_features_excluded`,
  `notes_write_waves_to_elf`, `notes_asm_parser_grammar`,
  `notes_differential_testing`, `notes_elf_reader`,
  `notes_magic_numbers_proposal`, `notes_seqc_parser_grammar`,
  `notes_splitreg_loop_model`, `notes_writeToNode_block_d_protocol`,
  `notes_libcpp_abi`, `notes_optimization_passes`.
- Two MIXED files moved to EXCLUDED instead of promoted:
  `placeholder_field_names.md` (audit log of completed
  placeholder-field renames; tracking artifact) and
  `struct_layouts.md` (1925-line cumulative offset table whose
  content has largely migrated into per-header struct comments in
  canonical `.hpp`s).  EXCLUDED set now totals 10 files.
- Verify-then-write per AGENTS.md caught one notes-only field-name
  discrepancy (IF-254: `Variable::flags` was described as
  `uint16_t` with Frozen=bit 1; canonical
  `runtime/resources.hpp` declares `int16_t` with Frozen=bit 8).
  Notes file fixed; no source change needed.
- 1603/1603 tests passing; 0 doxygen warnings; build clean.

**D7 (verify-triage sweep) complete:**

- Burned down the doc-accuracy backlog accumulated through D2–D6.
  Final tag counts vs targets: `\unclear=1` (≤2 ✓),
  `\verifyme=0` (≤3 ✓), `\binarynote=40` (≤40 ✓).
- Round 3 triaged 33 sites across 4 commits (`2b4d43d` asm+infra,
  `c8df0ca` ast+core+waveform, `78b1a5d` codegen+runtime+io+device,
  `e4b93c8` final trim).  Roughly 13 Drops (redundant with `\brief`
  / `\details`, or pure reconstruction-history meta), 14 Tightens
  (most to `\note` for device-gating and behavior detail; one to
  `\warning` for a thread-safety caveat), Keeps for genuine
  caller-visible API surprises (e.g. operator asymmetry, mid-name
  vs behavior mismatches, parameter-name asymmetries in error
  messages, libc++ destructor exception swallowing).
- New `\unverifiable` tag added (commit `383ba8f`) to distinguish
  permanently-unverifiable hypotheses from `\verifyme` work that
  can still be done.  Five sites converted: `NodeType::Table` arm
  (IF-249), `ddSectionIndex_` write-only field, `AsmExpression::str`
  with no external caller, `Severity` enum integer mapping.
  `\unverifiable_list` page joins the cross-reference set.
- 1603/1603 differential tests passing throughout; 0 doxygen
  warnings at every commit.

**D8 (coverage-gap tests for latent prefetch paths) complete:**

- The IF-223 / IF-244 reconstructions originally listed three
  candidate test cases (Play `cervino_indexed_nonsplit`, Table
  sub-path C2, `playWaveTable` with non-empty cache).  D9.1 / D8
  scoping reframed all three: IF-246 GDB-confirmed the
  `0x1db4ad..0x1db55d` block is reached only via the Load
  dispatch (no Play counterpart exists); IF-249 GDB-confirmed
  `NodeType::Table` is unreachable from the public
  `compile_seqc(...)` binding (only `Node::from_json` produces
  Table nodes); `playWaveTable` does not exist as a SeqC API.
- Only one new test case was authorable:
  `core:uhfawg_load_cervino_prf_path_b1` covers Load Path B1
  (`cachePtr->size_ == waveformMemorySize`) and graduated
  IF-246's reconstruction from "static-only verified" to
  "test-verified" once D10 landed the full Path B1 emission.
- Tests 1602/1602 → 1603/1603 (added `path_b1`); 0 doc warnings.

**D9 (resolve IF-244 dead label blocks and Table-C-split wprf gate)
complete:**

- D9.1 (commit `f97effd`): GDB-traced the original binary at
  `0x1db4ad..0x1db55d` and disproved IF-244's Play-side claim — the
  block is Load-side Path B1 only.  Promoted finding to **IF-246**.
- D9.2 (commit `8423555`): GDB-confirmed the Table-C-split `wprf`
  gate at `0x1db92e` reads `AWGCompilerConfig::isHirzel` (offset
  `+0x18`) and skips `wprf` when set.  UHFAWG (isHirzel=0) emits
  wprf; HDAWG/SHFSG/SHFQC_SG/SHFLI (isHirzel=1) skip.  Promoted to
  **IF-247**.  A separate "isHirzel inversion" alarm raised during
  D9.1 was investigated and dismissed as a naming-convention
  confusion (**IF-248**); recon's per-device assignments in
  `awg_device_props.cpp` already match the binary.
- D9.3 (commit `d1515c8`): deleted the two dead label-only blocks
  (`play_cervino_indexed_nonsplit` at the old
  `prefetch_placesingle.cpp:865-941`, `table_indexed_with_clamp` at
  `:951-1022`); promoted the `\verifyme` at the Table-C-split
  inline emission to a real `if (!config_->isHirzel)` gate.
  Backlog tag deltas: `\verifyme` 16→13, `\binarynote` 81→80.
- D9.4 (commit `d0b4170`): dropped — extracting a shared
  `emitPrfEpilogueAndInsert_` helper would have to wrap so much
  conditional behaviour (clamp vs no-clamp, gated vs unconditional
  `wprf`) that it would obscure rather than clarify.
- 1602/1602 tests passing throughout; 0 doxygen warnings.

**D10 (reconstruct `load_cervino_prf` Path B1 fully) complete
(commit `a6f92d9`):**

- Replaced the incomplete stub at `prefetch_placesingle.cpp:342-349`
  with the full binary emission shape per IF-246:
  `prf → 2x addi → wprf → prf → jmp load_finalize`.
- Correction folded into the recon: the local `wprf` at `0x1db4bd`
  is **unconditional**, not gated on `!isHirzel`.  Only the
  load_finalize tail `wprf` is gated per IF-247.  UHFAWG (the only
  device reaching this path) therefore emits **two** `wprf`
  instructions on this code path; a `\verifyme` was added near the
  local `wprf` to flag the expectation.
- Test verification: `core:uhfawg_load_cervino_prf_path_b1` (added
  under D8) was authored red against the old stub (orig=76 vs
  recon=60 bytes of `.text`); the rewrite makes it pass
  byte-identical.  Disassembly was delegated to a subagent and the
  proposed C++ block matched the binary on first build (no
  iteration needed).  Tests 1603/1603.

**D-AUDIT-2 (swap layout-misnamed `AWGAssemblerImpl` string slots)
complete (2026-05-12, commit `282bd16`):**

- Promoted from IF-250.  Renamed `+0x20` slot → `unusedStr020_`
  and `+0x38` slot → `asmSource_` in the `AWGAssemblerImpl` header
  (layout comment and field declarations both updated).  Ctor
  init list in `awg_assembler_impl.cpp` reordered to match the new
  physical offset order.  Pipeline writer (`assembleFile`) and
  reader (`writeToFile`) in `awg_assembler_impl_pipeline.cpp` now
  correctly target the `+0x38` slot by name as well as by data
  flow.  Dropped the `\unclear` from the new `+0x20` slot's brief.
  IF-250 marked **fixed**.  Tests 1603/1603 with no ELF byte
  changes.

**D-AUDIT-3 (mirror binary's `apiErrorMessages` anon-namespace
table) complete (2026-05-12, commit `ac0fc85`):**

- Promoted from IF-251.  Introduced an anonymous-namespace
  `apiErrorMessages` `std::map<int, std::string>` in
  `reconstructed/src/core/error_messages.cpp`, populated at
  static-init time by an `ApiErrorMessagesInitializer` struct with
  all 52 entries the binary's BSS table at `0xb85230` holds
  (16384–16389 minus 16388; 32768–32800; 36864–36877).
  `getApiErrorMessage` now reads from this dedicated table instead
  of from `ErrorMessages::messages`, restoring the binary's
  two-independent-tables data-flow shape.  Strings are duplicated
  literals (faithful to the binary), not copy-derived from
  `messages`.  IF-251 marked **fixed**.  Tests 1603/1603 (no
  observable change since the two tables agree on shared keys).

**D17 (reconstruct `zi_environment` cluster) complete (2026-05-12):**

- Replaced the constant-`false` `runningOnMfDevice()` stub at
  `core/stubs.cpp:34` with a proper rebuild of the 8-symbol cluster
  in `reconstructed/{include,src}/io/zi_environment.{hpp,cpp}`.
  Public surface: `runningOnMf{,64}Device(){,(string const&)}`,
  `hasMediaDevNode(string const&)`, `makeDirectories(fs::path const&)`,
  `markFileHidden(fs::path const&)`,
  `initBoostFilesystemForUnicode()`.  Anon-namespace helpers
  (`readManifestImpl`, `doIsMf`, `isMf`, `isMf64`, `laboneManifest`)
  mirror the binary's helper set; cached `bool` Meyers singletons
  back the 0-arg detection forms; path-arg overloads bypass the
  cache and parse fresh.
- IF-253 upgraded from "narrative scrubbed" to **fully resolved**.
- IF-257 filed and immediately fixed: a dead anon-namespace copy
  of the manifest helpers had been sitting in `core/platform.cpp`
  with a partially-wrong `isMf64` body (claimed bare
  `platform.size() == 10`; binary actually XOR-checks
  `platform == "linuxARM64"` byte-for-byte).  Removed the
  duplicate; sole authoritative implementation now lives in
  `io/zi_environment.cpp` with the verified `linuxARM64` check.
- `zi_folder.cpp` now `#include`s `zhinst/io/zi_environment.hpp`
  instead of forward-declaring `runningOnMfDevice()`.
- Tests 1603/1603 (no behavioural change on a PC test-host: the
  manifest is absent, so the cached `bool` is `false`, matching
  the prior stub's verdict).  Doxygen 0 new warnings; coverage
  95.2% (2941/3088 symbols, 8 newly documented).

**D18 (reconstruct `ast_misc` cluster, per-subclass clone
virtualisation) complete (2026-05-12):**

- **Audit outcome (IF-258).**  The original TODO premise was based
  on a misread.  `Node::clone()` at `src/ast/node.cpp:221` is the
  codegen IR `Node`, not the AST base.  The actual AST base
  `SeqCAstNode::doClone()` was already pure virtual at
  `seqc_ast_node.hpp:191`, with all 53 subclasses already
  overriding it via the family macros (`SEQC_TRIVIAL_LEAF_IMPL` ×6,
  `SEQC_UNARY_IMPL` ×5, `SEQC_OPERATOR_IMPL` ×22,
  `SEQC_BINARY_IMPL` ×5, `SEQC_LIST_IMPL` ×4) plus 11 hand-written
  overrides — matching the binary's 53 `SeqC*::clone() const`
  symbols.  No silent-slicing risk existed.
- Naming asymmetry retained: recon spells the slot `doClone()`,
  binary spells it `clone()`; the recon-side rename intentionally
  disambiguates from the codegen IR `Node::clone()`.
- Real fix: `SeqCRepeat`'s first-child accessor renamed `count` →
  `cond` to match the binary's `SeqCRepeat::cond() const` symbol
  at `0x203b80`.  Updated `SEQC_BINARY` / `SEQC_BINARY_IMPL`
  instantiations and the sole caller in
  `seqc_ast_eval_control.cpp:2368`.  Added a `\binarynote` to the
  header explaining the binary's quirky accessor name (the field
  is semantically a count, but spelled `cond`).
- Tests 1603/1603; 0 new doxygen warnings.

**D19 (small-cluster bundle audit & closure) complete (2026-05-12):**

Per IF-259, the D19 cluster list (~28 symbols across 10 clusters)
was triaged and resolved with one stub fill plus four docstring
upgrades.  The remaining ~24 symbols are deferred to the grand
finale under the `\unverifiable` regime per the D16 caveat (zero
difftest callers, semantically-equivalent-via-different-shape, or
auto-emitted MI thunks).

Per-cluster outcomes:

| Cluster | Symbol(s) | Outcome |
| ------- | --------- | ------- |
| `random::infra` | `Random::seedRandom()` | Already covered by `seqc_libcxx_mt19937_seed_urandom` shim; doc-only. |
| stub-only user code | `WavetableFront::dummyWarning` | Stub filled — `LogRecord(Warning) << "Warning not tracked: " << msg`. |
| stub-only user code | `tracing::TraceProvider::~TraceProvider()` | `= default` is exact-equivalent (member shared_ptr dtor); doc-only. |
| stub-only user code | `SeqCIfElse::operator=`, `SeqCCondExpr::operator=` | Already correct copy-and-swap; binary inlines `doClone` differently; doc-only. |
| `exceptions::core` | 13 `boost::wrapexcept` thunks | MI offset thunks, auto-emitted, no source change. |
| `misc::?` | 2 helpers | Three of five symbols covered: `ErrorCodeTraits<ErrorCode>::successCode`/`defaultMessage` (template specialisations defined out-of-line in `core/exception.cpp`; mangling matches except template-arg portion since recon uses `ErrorCode` stand-in vs binary's `boost::system::error_code`), and TLS init wrapper `_ZTHN6zhinst15GlobalResources6randomE` (auto-emitted by gcc once `random` was given a function-call initializer; ctor-side re-seeding removed → matches binary's once-per-thread guard).  Two `getKind` overloads remain deferred (zero callers in recon, would require ~200 LoC of fake `boost::system::error_code` + `singleErrorKindCategory` infrastructure).  F-followup, 2026-05-16. |

- Tests 1603/1603 (the `dummyWarning` change is invisible to the
  suite because every test installs a real callback).  Doxygen 0
  new warnings.

**D16 (diagnostics_text cluster reconstruction) complete (2026-05-12):**

Twenty user-facing text helpers — HTML/XML/JSON/CSV escaping, URL
rewriting, filename sanitisation, UTF-8 truncation — were
reconstructed from disassembly in `core/diagnostics_text.{hpp,cpp}`.
The cluster had zero callers in the reconstructed compiler and no
test path through the Python bindings, so every public function
carries `\verifyme` pending validation by the new Phase E diff-test
harness.

Highlights:

- New files: `reconstructed/include/zhinst/core/diagnostics_text.hpp`
  (declarations + doc comments) and
  `reconstructed/src/core/diagnostics_text.cpp` (definitions).
- Disassembly notes: `reconstructed/notes/diagnostics_text.md`
  (per-symbol bodies, .rodata literals, regex patterns, replacement
  tables, pseudo-C sketches; ~2000 lines).
- `zhinst::quote` was already reconstructed in
  `src/core/platform.cpp:193`; the D16 header does NOT redeclare it
  (IF-261).
- Two `\binarynote` entries record preserved binary quirks:
  `xmlEscapeUtf8Critical` emits `&#-NNN;` for high bytes due to a
  signed-extension fold; `sanitizeInvalidFilename`'s Windows
  reserved-name regex `COM[1-9]|PRN` lacks `LPT[1-9]`/`AUX`/`CON`/`NUL`
  and lacks anchors.
- IF-262: `xmlUnescape` recon omits the secondary
  `escapeMaliciousXmlEscapedSequences` post-pass observed in the
  binary; deferred to Phase E.
- Tests 1603/1603 pass; 0 new doxygen warnings.  Backlog deltas:
  `\verifyme` 1 → 24, `\binarynote` 18 → 20.

With D16 closed, every D-phase is complete.  The next phase is
**Phase E** (diff-test harness for binding-unreachable
reconstructions) — see `TODO.md` for the E1/E2/E3 breakdown.

## Phase E — diff-test harness for binding-unreachable reconstructions

The Phase D coverage push exposed a structural gap: many recon
symbols are unreachable from the Python `compile_seqc()` entry
point and therefore invisible to the 1603-case differential test
suite.  These functions carry `\verifyme` because they have no
behaviour-level proof against the binary.  Phase E built a second
differential harness (`tests/diff_unreachable/`) that loads
`_seqc_compiler.so` *and* a recon-only shared library
(`reconstructed/build-libcxx-test/libdiagtxt_libcxx.so`) into the
same process and invokes matched symbol pairs directly via
`ctypes` + a small C-ABI trampoline shim, comparing return values
and exception outcomes.

Highlights:

- New driver: `tests/diff_unreachable/harness.py` (symbol table,
  ABI-shape dispatch, corpus inputs, both-threw collapse for RTTI
  divergence between statically-linked orig and dynamically-linked
  candidate).
- New shim: `tests/diff_unreachable/shim.cpp` (exception-safe
  trampolines so a candidate-side throw never aborts the harness).
- New build system: `reconstructed/CMakeLists-libcxx-test.txt` —
  a separate cmake project that builds the same recon TUs against
  libc++ (matching the orig's ABI) into a shared library that the
  harness can `dlopen`.
- Initial coverage at Phase E close: ~944 cases across the
  diagnostics-text module (`core/diagnostics_text.cpp`), exercising
  ~30 symbols.  Several IFs surfaced and were fixed during the
  push (IF-265..IF-269).

## Phase F — Backlog hardening

Phase F clears inherited tag-debt (`\binarynote` audit) and
extends the diff-unreachable harness to a second module, applying
verify-then-write throughout.

- **F1 (`\binarynote` audit).**  Closed 2026-05-13.  All 26 in-tree
  `\binarynote` sites across 17 files audited against the binary.
  One IF surfaced and fixed (IF-269 — `Node::type2str` recon
  emitted `"unknnown"` (3 n's); binary emits `"unknown"` (2 n's)).
  Remaining 25 sites all verified accurate.

- **F2 (harness expansion to `infra/calver.cpp`).**  Closed
  2026-05-13.  Reconstructed 4 missing calver symbols
  (`getLaboneVersion`, `getLaboneVersionWithCommitHash`,
  `fromDecimal(string const&)`, `extractVersionTriple`); added 7
  new harness ABI shapes (`sret_void`, `sret_blob_void`,
  `sret_blob`, `sret_blob_u32`, `ref_blob`, `ctor_blob_cref`,
  `sret_blob_cref_throws`); covered 20 of 21 unique calver
  symbols (deferred: `operator<<(ostream, CalVer)` — needs a fake
  ostream shape).  Yield: IF-270 (`extractVersionTriple` missing
  outer `try/catch` + wrong `token_compress_mode` enum value;
  GDB-verified, fixed in commit `2d97867`).

  Net harness coverage at F2 close: 957/957 cases.  Main test
  suite: 1603/1603 (unchanged).

- **F3 (triage + wrap-up).**  Closed 2026-05-13.  Picked up the
  one F2 deferral by adding the `cref_ostream_throws` harness
  shape: shim helpers wrap a libc++ `std::ostringstream` (both
  sides link the same libc++, so a single instance is safe to
  pass to either) and a new `diff_unreachable_try_cref_ostream`
  trampoline applies the established raw-fn-pointer + 0/1/2
  exception convention.  Wired `operator<<(ostream&, CalVer
  const&)` at binary `0x100b40` using the existing CALVER blob
  corpus.

  Calver harness coverage now 21/21 unique exported symbols
  (was 20/21).  No new IF surfaced — the 13 CalVer inputs all
  produced byte-identical captured strings between original and
  candidate, confirming `toString(CalVer)` and `op<<` are both
  binary-faithful.

  Net harness coverage at F3 close: 970/970 cases (was 957;
  +13).  Main test suite: 1603/1603 (unchanged).

- **F4 (harness expansion: `device/awg_device_props.cpp`).**  Closed
  2026-05-13.  Added the three POD-arg / (POD-or-string)-return
  symbols from `awg_device_props.cpp` to the diff-test harness:
  `toAwgDeviceType(DeviceTypeCode, AwgSequencerType)`,
  `toString(AwgSequencerType)`, and
  `makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode,
  AwgSequencerType)`.  Three new ABI shapes (`pod_u32_2u32`,
  `sret_str_u32`, `sret_str_2u32`); `awg_device_props.cpp` added to
  `CMakeLists-libcxx-test.txt` (sole new dep `device_type.hpp`
  already present).  Original-side targets are `.hidden`, resolved
  by raw offset.  Corpora cover the full `DeviceTypeCode` 0..32 plus
  out-of-range probes, crossed with `AwgSequencerType` {Auto, QA,
  SG, out-of-range}: +284 cases, all PASS, no new IF.  The
  `getAwgDeviceProps<*>` family is deferred — `AwgDeviceProps`
  carries `std::string` fields, requiring a destructor-aware
  per-field-string-decode shape.

  Net harness coverage at F4 close: 1254/1254 cases (was 970;
  +284).  Main test suite: 1603/1603 (unchanged).

- **F5 (harness expansion: `getAwgDeviceProps<T>` family).**  Closed
  2026-05-13.  Added all 9 `getAwgDeviceProps<T>` template
  specializations from `awg_device_props.cpp` to the diff-test
  harness via a new shape `sret_props_cref` —
  `void f(AwgDeviceProps* sret, DeviceType const* dt)`.  The
  `AwgDeviceProps` slot (0x80 B) embeds 4 `std::string` fields, so
  the shape decodes the slot field-by-field (POD scalars by direct
  read; strings via the existing libc++ shim helpers applied to
  interior offsets) into a 9-tuple, which normalises away the
  heap-pointer differences that would otherwise dominate every
  byte-blob diff.  Cleanup runs `~AwgDeviceProps()` in place via
  a new shim helper.  Five new shim helpers added
  (`_devicetype_make`/`_free`,
  `_awgprops_alloc_uninit`/`_free_uninit`/`_destroy_in_place`);
  `generic_device_type.cpp` added to `CMakeLists-libcxx-test.txt`
  (transitive dep of the production `DeviceType(string,
  vector<string>)` ctor).  Corpora: HDAWG (sole spec consulting
  `dt`, calling `dt.hasOption(ME)`) fed HDAWG{8,4} × {none, ME} =
  4 cases; the other 8 dt-ignoring specs each fed 1 placeholder
  DeviceType.  +12 cases, all PASS, no new IF — the recon's
  per-spec field assignments are byte-identical with the binary
  even where the function-body shape differs significantly (recon
  ~0x40-0x200 B vs binary ~0x280 B, the difference being
  whole-program inlining of the `AwgDeviceProps` ctor and per-field
  `std::string` ctors, not behavioural divergence).

  Net harness coverage at F5 close: 1266/1266 cases (was 1254;
  +12).  Main test suite: 1603/1603 (unchanged).

- **F-followup (harness: `ZiFolder::folderPath`).**  Built a new
  `sret_zifolder_2cref` harness shape exercising the `this`-bearing
  `string ZiFolder::folderPath(string const&, string const&) const`
  binding-unreachable method (`@0x2ce2f0`).  The shape reuses the
  existing 24-byte placement-construct slot mechanism since
  `ZiFolder` is layout-equivalent to a single `std::string` at
  offset 0 (`basePath_`).

  The first run uncovered three independent recon divergences from
  the binary, all logged as **IF-272** (fixed):
  1. The vendor-segment branch was inverted (recon always inserted
     `Zurich Instruments`; binary inserts it only when `subdir` is
     NOT `"/data"` or `"/settings"`).
  2. The vendor literal had a stray `'$'` prefix from misreading
     the libc++ short-string SIZE byte (`0x24 = 36 = (18 << 1)`)
     as a data character.
  3. `basePath_` was guarded by a spurious `!empty()` check that
     the binary does not have (only `extra` is non-empty-guarded).

  The corresponding `\brief` and supporting comment block in
  `zhinst/io/zi_folder.hpp` were also corrected (the brief had the
  vendor-segment condition stated backwards).

  Net harness coverage: 1283/1283 cases (was 1266; +17).  Main
  test suite: 1603/1603 (unchanged — none of the SeqC inputs
  reach `folderPath` in a byte-observable way, which is why the
  bug went undetected before the harness was built).

- **F-followup (audit: `io/zi_folder.cpp` + `io/zi_environment.cpp`).**
  Disasm-audited every reconstructed function in both files
  against the binary as a defensive sweep after IF-272, looking
  for further latent SSO-byte misreads.  Eleven functions
  audited; 9 verified clean (`ZiFolder` ctor, `ziFolder` factory,
  `sessionSaveDirectoryName`, `readManifestImpl`,
  `laboneManifest`, `doIsMf`, `isMf`, `isMf64`, both cached and
  both string-arg `runningOnMf*Device` overloads, plus the
  `markFileHidden` / `initBoostFilesystemForUnicode` Linux
  no-ops); 2 had divergences:

  - **IF-273 (`makeDirectories`, fixed).**  Recon wrapped the
    body in a spurious outer `try { ... } catch (std::exception
    const&)` that translated the message to `"Could not create
    directory ..."`; the binary has no `__cxa_begin_catch` in
    this function and emits only the
    `"Could not access directory '<dir>'."` message.  Recon also
    threw with the default sentinel error code `0x8000`; the
    binary throws with the explicit code `0x8011` (a
    `\binarynote` was added because the matching enum slot
    `ApiBufferTooSmall` is semantically inappropriate for a
    directory-permissions failure — but that's the value the
    binary really emits).
  - **IF-274 (`hasMediaDevNode`, cosmetic).**  Binary calls
    `boost::filesystem::status` twice on the same path/ec pair;
    recon calls it once.  Output identical for every input;
    recorded for future audits but not changed.

  Conclusion (IF-275): the IF-272 misread was an isolated
  incident, not a systemic pattern.  The vendor literal
  `"Zurich Instruments"` was unusually exposed because its size
  byte `0x24` falls in the printable-ASCII range; every other
  SSO literal in this code (`"device"`/`0x0c`,
  `"platform"`/`0x10`, `"/dev"`/`0x08`, `"0"`/`0x02`) encodes
  to a sub-`'!'` byte that is visibly metadata.

- **F-followup (harness expansion: `util::wave::hash`).**
  Added `util::wave::hash(string const&)` to the diff-unreachable
  harness via a new `sret_vec_u32_cref` shape.  The function
  returns `vector<unsigned int>` (24 B sret slot, 3-pointer libc++
  layout) holding the SHA-1 digest of a file's contents; the
  shape exercises full file-I/O + boost-SHA-1 + open-fail-IV
  paths.  Shim helpers added: `vec_u32_alloc_uninit`,
  `vec_u32_destroy_in_place`, `vec_u32_data`, `vec_u32_size`.
  `util_wave.cpp` registered in `CMakeLists-libcxx-test.txt`.
  Corpus has 14 inputs covering 0/1/63/64/65/1023/1024/1025/4096-byte
  files, NUL/0xFF/all-bytes payloads, and the missing-file path.
   All 14 byte-equal between original and recon (1297/1297 harness
   cases total, was 1283).  Main test suite unchanged at 1603/1603.

- **F-followup (harness expansion: `util::wave` POD batch + `hash2str`).**
   Extended the diff-unreachable harness to cover the rest of
   `util::wave`: `hash2str`, `double2awg`, `double2awg1m`,
   `double2awg16`, `awg2double`, `awg2marker`, `awg2double16`.
   Added six new shape kinds — `sret_str_vec_u32_cref` (string
   sret from `vector<u32>` arg), `pod_u16_double_u32` (uint16
   from double+uint32), `pod_u16_double` (uint16 from double),
   `pod_double_u16` (double from uint16, with NaN-aware bit-equal
   compare), `pod_u8_u16` (uint8 from uint16), and
   `pod_double_u32` (double from uint32, NaN-aware).  Shim now
   ships `vec_u32_make` / `vec_u32_free` so the `hash2str` corpus
   can synthesise digest vectors directly without chaining
   through `hash()`.  `src/core/stubs.cpp` (which actually
   contains the binary-verified bodies of `awg2*` and `hash2str`)
   pulled into `CMakeLists-libcxx-test.txt`.  Corpora total 262
   new cases: 10 hash2str payloads, 17 double samples × 6 markers
   for `double2awg{,1m}` (102 each), 17 for `double2awg16`,
   12 each for `awg2double` / `awg2marker`, 7 for `awg2double16`.
   Coverage includes NaN, ±Inf, ±0, denormals, and the SSE2
   `maxsd`-clamp boundary.  Harness total now 1559/1559 (was
   1297).  Main test suite unchanged at 1603/1603.

   **Incidental fix (IF-277):** the harness immediately uncovered
   a real recon bug — `double2awg` and `double2awg1m` used
   `std::max(-1.0, sample) * kFullScale`, which mishandles NaN
   (returns -1.0 rather than NaN).  The binary uses a single
   `maxsd xmm0(=-1.0), xmm1(=sample)` instruction whose semantics
   propagate NaN from the second source — the same pattern
   `double2awg16` already used.  Fix replaces `std::max` with
   `_mm_max_sd` in both functions.  Bug was unreachable from
   user SeqC (samples come from in-range generators); detected
   only via the harness's NaN corpus.

- **F-followup (harness expansion: `numeric::core` cluster).**
   Reconstructed and harness-covered the three `zhinst::numeric`
   helpers — `almostEqual`, `toRawByteArray`, `fromRawByteArray`
   — none of which are reachable from the Python bindings.

   - **New files**: `reconstructed/include/zhinst/core/numeric.hpp`
     and `reconstructed/src/core/numeric.cpp`.  The two raw-byte
     helpers take `std::span` arguments and are gated on
     `__cplusplus >= 202002L` so the main C++17 build silently
     omits them (the libcxx-test build is C++20).  `almostEqual`
     is always visible.
   - **Implementations**:
     - `almostEqual(double, double) → bool`: returns
       `boost::math::epsilon_difference(a, b) <= 1.0`, which
       compiles to the same SSE2 sequence as the binary.
     - `toRawByteArray(string_view, span<uint8_t>) → bool`:
       bounded `memmove` + NUL-terminate; returns true iff the
       source plus its NUL fit entirely in the destination.
     - `fromRawByteArray(span<uint8_t const>) → string_view`:
       returns a view from the span's start up to the first NUL
       (or the full span if no NUL is present).
   - **Harness shapes added** (`tests/diff_unreachable/harness.py`):
     `pod_bool_2double`, `pod_bool_strview_spanu8`, `strview_spanu8`.
     The last is the first kind to exercise libffi's 16-byte
     aggregate return convention (`%rax`+`%rdx` per SysV
     INTEGER+INTEGER), modelled via a local
     `_StringViewRet(c_uint64, c_uint64)` `Structure`.
   - **Coverage added**: 19 + 15 + 10 = 44 new harness cases.
     Curated corpora exercise NaN/±Inf/±0/ULP-boundary pairs for
     `almostEqual`, dst-buffer overflow/exact-fit/empty/embedded-
     NUL for `toRawByteArray`, and NUL-at-start/mid/end + no-NUL
     for `fromRawByteArray`.
   - **Result**: 1603/1603 main + 1603/1603 harness (was 1559).
     No incidental binary divergence found — recon matches binary
     bit-for-bit on the curated corpus.

- **F-followup (harness expansion: `base64::infra` cluster).**
   Reconstructed and harness-covered the single `zhinst::base64::encode`
   helper — standard RFC 4648 base64 codec, 910 B in the binary.

   - **New files**: `reconstructed/include/zhinst/core/base64.hpp`
     and `reconstructed/src/core/base64.cpp`.  Span-using API
     C++20-gated so the main C++17 build silently omits it.
   - **Implementation**: direct `std::string::push_back` build using
     the standard alphabet (located at `.rodata` 0x90cf90 in the
     binary).  The original uses `std::ostringstream` internally,
     but only the emitted byte sequence is observable at the API
     boundary; harness verifies bit-identity.
   - **Harness shape added**: `string_spanu8` —
     `string f(span<uint8_t const>)` with sret in `%rdi` and span
     `(data, size)` in `(%rsi, %rdx)`.  Reuses the existing libc++
     uninit-slot helpers (`alloc_uninit_slot`/`string_bytes`/
     `destroy_and_free_slot`).
   - **Coverage added**: 23 cases — empty, all three pad classes
     (n%3 ∈ {0,1,2}), RFC 4648 examples, all-bits-set bytes, the
     22-byte SSO boundary, and a 64-byte payload that pushes the
     result into long-string form.
   - **Result**: 1603/1603 main + 1626/1626 harness (was 1603).
     No incidental binary divergence — recon matches binary
     bit-for-bit on the full corpus.

- **F-followup (cluster: `compiler_helpers::codegen`).**
   Promoted `AWGCompilerImpl::nodeListToJson` (the 796 B
   `.nodes_json` builder, binary symbol @ `0x1088d0`) from an
   inlined block inside `writeToStream` into a real public
   member function matching the binary signature.

   - **Edits** (`reconstructed/src/codegen/awg_compiler.cpp`):
     added the `nodeListToJson(vector<NodeMapItem> const&,
     unordered_map<NodeMapItem, set<AccessMode>> const&) const`
     declaration to the `AWGCompilerImpl` class, factored the
     pre-existing inlined body (lines 1182–1198 in commit
     `2bfaeec`) into the new definition, and replaced the
     `writeToStream` call site with a one-line invocation +
     `boost::json::serialize`.
   - **Body**: walks the input vector, calls
     `NodeMapItem::toJson()` per node, looks each node up in the
     mode map, and when present attaches a `"modes"` array of
     `boost::json::string` entries (`"soft"` / `"direct"` /
     `"custom"`) sourced via `toString(AccessMode)`.  Returns
     `{"nodes": [...]}`.
   - **Intentional divergences from the binary** (logged in
     IF-278): the binary builds an intermediate
     `std::set<AccessMode>` from `it->second` before iterating to
     emit `"modes"`; recon iterates `it->second` directly because
     the source is already a sorted set and the copy is
     observationally a no-op.  The binary also constructs the
     mode strings directly via the
     `boost::json::string(string_view, storage_ptr)` overload
     using a static SSO table at `.rodata 0x9573c0`, while recon
     routes through `std::string`.  Both routes emit identical
     JSON byte sequences.
   - **No new harness needed**: this is a member function called
     from `writeToStream` along the normal compile pipeline, so
     it is exercised by the existing 1603 difftest cases that
     produce `.nodes_json` sections.
   - **Result**: 1603/1603 main + 1626/1626 harness — no
     observable byte difference before vs. after the
     extraction.  Cluster count: ~10 → ~9 deferred helpers.

- **F-followup (cluster: `random::infra`).**
   Promoted the binary's `Random::seedRandom()` (the 297 B
   `/dev/urandom` PRNG seeder, binary symbol @ `0x16be80`) from
   an `extern "C"` shim (`seqc_libcxx_mt19937_seed_urandom`) into
   a real mangled C++ method matching the binary signature.

   - **New files**: `reconstructed/include/zhinst/infra/random.hpp`
     declaring `class Random { void seedRandom(); uint64_t state_[313]; }`
     — a thin typed view onto the 313 × `uint64_t` mt19937_64
     state.  Definition lives in `prng_libcxx.cpp` (the existing
     libc++ shim TU) so the `std::mt19937_64` seed-expansion
     matches the binary byte-for-byte.
   - **Edits**:
     - `prng_libcxx.cpp` gained the `Random::seedRandom()` body
       and lost the now-redundant `seqc_libcxx_mt19937_seed_urandom`
       C-shim.  The `/dev/urandom` read is implemented as two
       `fread(uint32_t)` calls combined into a `uint64_t` —
       observably identical to libc++'s
       `random_device("/dev/urandom") + uniform_int_distribution<uint64_t>`
       sequence the binary uses, but without taking a libc++.so
       runtime dependency.
     - `custom_functions_playback.cpp::randomSeed` now calls
       `reinterpret_cast<Random*>(GlobalResources::random)->seedRandom()`,
       mirroring the binary's call shape at `0x149832`.
     - `CMakeLists.txt`: added `-I include` to the
       `prng_libcxx_obj` custom-build command so the shim TU can
       see the new header.
     - `custom_functions.hpp::randomSeed` doc comment updated to
       reference `Random::seedRandom()` in place of the old shim.
   - **Pre-fix divergence (logged in IF-279)**: the old C-shim
     read 4 bytes (uint32) from `/dev/urandom`; the binary reads
     8 bytes (uint64).  Invisible to difftest because no test
     case combines `randomSeed()` with subsequent `randomGauss()`
     / `randomUniform()`.  The new implementation matches the
     binary's 8-byte-read shape.
   - **No harness coverage**: `seedRandom`'s output is
     non-deterministic by design (reads `/dev/urandom`); a
     deterministic-mode harness would need fd-injection or
     `LD_PRELOAD` plumbing that no current scenario justifies.
   - **Result**: 1603/1603 main + 1626/1626 harness — no
     regressions.  Cluster count: ~9 → ~8 deferred helpers.


- **2026-05-16** F-followup: cluster `awg_config::device` (1
  helper, 254 B — `AwgPathPatterns(AwgPathPatterns const&)` @
  0x2cc4f0) **closed as ABI-divergence by design** (IF-280).
    - **Symptom**: D14 inventory flagged
      `_ZN6zhinst15AwgPathPatternsC2ERKS0_` as absent in the
      recon; only the 3-arg ctor is emitted out-of-line.
    - **Root cause**: header declares the copy ctor `= default`
      (correct C++ semantics).  Whether the compiler emits it
      out-of-line is a pure inlining decision; under `-O2` with
      libstdc++, gcc inlines it at every callsite.  The binary's
      body is the textbook **libc++** 3-string copy sequence
      (SSO bit-0 test, inline 24-byte `movups`, fallback
      `__init_copy_ctor_external`), which cannot be reproduced
      from a libstdc++ TU because the string layout differs.
    - **Why no fix**: a libc++ shim TU (cf. `Random::seedRandom`)
      would emit a libc++-string symbol with no caller, since
      every recon caller (`getAwgDeviceProps<*>` family +
      `_GLOBAL__sub_I_properties.cpp`) is libstdc++-built.
      Forcing out-of-line emission of the libstdc++ ctor would
      produce a different body and a different mangling than the
      binary, neither matching nor filling the inventory gap.
    - **Result**: no source change.  Tests unchanged (1603/1603
      main + 1626/1626 harness).  Deferred-cluster count: ~8 → ~7.

- **2026-05-16** F-followup: cluster `device_option::device` (2
  helpers, 90 B — `DeviceType::deviceType()` @0x2d2c20 +
  `DeviceTypeImpl::doClone()` @0x2d3280) **fixed by rename**
  (IF-281).
    - **Symptom**: D14 inventory flagged both symbols as absent.
      The recon had both functions but under different names
      (`impl()` and `clone()`); bodies were already
      semantically identical.
    - **Fix**: mechanical rename across 15 files — header
      decls, base impls in `device_type.cpp`, 33 subclass
      overrides spread across `device_subclasses.hpp` +
      `device_{ghf,hdawg,hf2,hwmock,mf,pqsc,qhub,shf,shfacc,uhf,unknown,vhf}.cpp`,
      plus 2 callsites in `DeviceType`'s copy ctor and
      copy-assignment.  `Node::clone` and `NodeMapData::clone`
      are unrelated and untouched.
    - **Result**: `_ZNK6zhinst10DeviceType10deviceTypeEv` and
      `_ZNK6zhinst6detail14DeviceTypeImpl7doCloneEv` now emitted
      (matching binary mangled names).  1603/1603 main; no
      harness-relevant change.  Deferred-cluster count: ~7 → ~6.
    - **Note**: cluster overview prose ("DeviceOption hash /
      compare") was wrong — actual symbols are the pimpl
      accessor and polymorphic deep-copy.  Re-confirms AGENTS.md
      rule: always cross-check the symbol-list section, never
      trust the cluster prose.

- **2026-05-16** F-followup: cluster `node_misc::core` (1
  helper, 43 B — `NodeMap::pauPoffIwrap` @0x1c5650) **fixed by
  promotion** (IF-282).
    - **Symptom**: D14 inventory flagged the symbol as absent.
      Recon already had the equivalent logic as an
      anonymous-namespace `wrap23` helper called only from
      `NodeMap::toPhase`.
    - **Fix**: promote `wrap23` to a public static
      `NodeMap::pauPoffIwrap(unsigned int)` (header + cpp);
      route `toPhase` through it.  Body is the textbook 23-bit
      two's-complement wrap with the `0x400000` lone-sign-bit
      saturation guard, matching `objdump` of @0x1c5650 byte
      for byte.
    - **Result**: `_ZN6zhinst7NodeMap12pauPoffIwrapEj` now
      emitted (matching binary mangled name).  1603/1603 main;
      no harness-relevant change.  Deferred-cluster count: ~6
      → ~5.
    - **Note**: the public helper is now available for any
      future PAU/POFF (Phase Accumulator Update / Phase Offset)
      immediate-encoding call-site that needs the same fold.

- **2026-05-16** F-followup: cluster `misc::?` (3 of 5 helpers
  closed; 2 deferred-by-design).
    - **Approach**: option 1 — take the three easy wins, defer
      the two `getKind(...)` overloads whose recon footprint
      would dwarf their value (zero callers, ~200 LoC of fake
      `boost::system::error_code` infrastructure).
    - **`ErrorCodeTraits<ErrorCode>::successCode`** and
      **`ErrorCodeTraits<ErrorCode>::defaultMessage`**: added
      `template <typename T> struct ErrorCodeTraits` to
      `core/exception.hpp` with the two member declarations,
      and out-of-line specialisations for the recon
      `ErrorCode` stand-in in `src/core/exception.cpp`.  The
      definitions are non-`inline` so the symbols are emitted
      even with no in-tree callers.  Bodies match
      `objdump` of @0x2ea150 / @0x2ea170 byte for byte; only
      the template-arg portion of the mangled name diverges
      (recon `IN6zhinst9ErrorCodeE` vs binary
      `IN5boost6system10error_codeE`), reflecting the
      pre-existing decision to use a stand-in `ErrorCode`
      rather than fake `boost::system::error_code`.
    - **TLS init `_ZTHN6zhinst15GlobalResources6randomE`**:
      changed `GlobalResources::random` from
      `uint64_t[313]` to
      `std::array<uint64_t, 313>` and added a
      function-call initializer (`seed_mt19937_64_state()` in
      an anonymous namespace in `global_resources.cpp`).
      gcc auto-emits both `_ZTHN...random` (init wrapper,
      strong, matches binary @0x1f6090) and `_ZTWN...random`
      (access wrapper, matches binary @0x1f6180).  The
      MT19937-64 seeding loop moves out of
      `GlobalResources::GlobalResources` into the dynamic-init
      function, **also fixing a behavioural divergence**
      (IF-283): the binary seeds once-per-thread (TLS+0xa18
      guard byte); recon previously re-seeded on every
      `GlobalResources` construction.  Three call sites that
      decayed the array to `uint64_t*` now use `.data()`
      (`custom_functions_playback.cpp:887`,
      `waveform_generator_dsp.cpp:988,1035`).
    - **Deferred**: `getKind(Exception const&)` @0x2e5180
      (189 B) and `getKind(boost::system::error_code const&)`
      @0x2e50d0 (170 B).  Both have zero recon callers; the
      latter additionally needs `singleErrorKindCategory`
      anon-namespace static plus `boost::system::detail::
      generic_cat_holder` interop.  Documented in IF-284 as
      *deferred-by-design*, not as bugs.
    - **Result**: 1603/1603 main; 1626/1626 harness.  D14
      deferred-cluster count: 1 cluster / ~5 helpers → 1
      cluster / 2 helpers (deferred-by-design).  All
      *closeable* D14 work is now done.

- **2026-05-16** D14 inventory refresh + opportunistic
  `ErrorCodeTraits<ErrorCode>::asException` add.
    - **Refresh**: re-ran the `nm`-difference sweep against
      the current recon `_seqc_compiler.so` (post `8770bf6`)
      to validate D14 cluster closure.  Truly-absent
      function count dropped from D14's 114 → **110**;
      delta is the net effect of all F-followups since
      D14.  Full breakdown appended to
      `reconstructed/notes/d14_inventory.md` "Refresh
      2026-05-16".  No latent bug surfaced; remaining
      surface is 54 `ErrorMessages::format<…>` template
      instantiations (informational), 13
      `detail::initializeSfcOptions<…>` (cluster
      candidate, no caller), 8 API-error-translation
      helpers (cluster candidate, no caller),
      `csv_waveform_2arg` (still deferred), C++20-gated
      symbols (`base64::encode`, `toRawByteArray`,
      `fromRawByteArray` — by design), and ~30 misc
      one-offs.
    - **`ErrorCodeTraits<ErrorCode>::asException`**: the
      one immediately actionable item from the refresh —
      sibling of `successCode`/`defaultMessage` already
      done.  Added to the trait template in
      `core/exception.hpp` plus an out-of-line
      specialisation in `src/core/exception.cpp` that
      reduces to `Exception{std::move(desc)}` (the
      binary's @0x2ea190 inlines the move + the
      moved-from string's conditional-delete on the
      unwind path; both shapes are observationally
      identical).  Added forward declarations of
      `GenericErrorDescription` and `Exception` ahead of
      the trait definition because the trait now
      references both.
    - **Result**: `_ZN6zhinst15ErrorCodeTraitsINS_9ErrorCodeEE11asException…`
      now emitted (recon mangling; template-arg portion
      diverges from binary as before by design).  Truly-
      absent function count: 110 → 109.  1603/1603 main;
      1626/1626 harness; 0 new doxygen warnings; doc
      coverage 95.2% (2970/3120, +6 from the
      `asException` addition).

- **2026-05-16** F-followup: cluster
  `detail::initializeSfcOptions<T,N>` (13 instantiations).
  Investigation found the header template was already a
  byte-for-byte correct reconstruction (verified against
  binary @ 0x2e0d50 for `<Hf2Option,6>` — `DeviceOptionSet`
  ctor + N×bit-test+insert).  All 13 instantiations were
  going un-emitted in `_seqc_compiler.so` because the body
  is small enough that every caller (`Hdawg{4,8}`,
  `Hf2{li,is}`, `Mf{li,ia}`, `Uhf{li,awg,qa,ia}`,
  `Shf{qa2,qc,li}`, `Ghfli`, `Vhfli`) inlined it during
  `-O0` codegen.  Fix: converted the header template to a
  forward declaration with `extern template` declarations
  for all 13 `<T,N>` pairs (suppresses implicit
  instantiation in every TU that includes the header), moved
  the definition into a new TU
  `reconstructed/src/device/device_sfc_options.cpp`, and
  emitted the 13 explicit instantiation definitions there.
  All 13 mangled symbols now appear (as `W` weak symbols —
  global vs. local linkage divergence is reconstruction-wide
  and out of scope here).  Symbol mangling diverges only in
  the `std::array` template-arg portion (recon `St5array`
  via libstdc++; binary `NSt3__15arrayE` via libc++) — the
  standard libcxx ABI mismatch noted throughout
  `d14_inventory.md`.  Qualified name + body shape match.
  Truly-absent function count: 110 → 97 (−13 net for the
  initializeSfcOptions cluster, since `asException` already
  closed one above).  1603/1603 main; 1626/1626 harness; 0
  new doxygen warnings.

- **2026-05-16** F-followup: `api_error_translation::core`
  ErrorKind subset (4 of 8).  Reconstructed
  `toApiCode(ErrorKind)` @ 0x2e5280,
  `make_error_condition(ErrorKind)` @ 0x2e50c0,
  `toZiErrorKind(ErrorKind)` @ 0x2e5240, and
  `fromZiErrorKind(ZIErrorKind_enum)` @ 0x2e5260 in a new
  TU `reconstructed/src/core/error_kind.cpp` with header
  `reconstructed/include/zhinst/core/error_kind.hpp`.  Per
  user direction, scope limited to the ErrorKind-only
  subset — avoids fabricating the
  `boost::system::error_code` / `error_category` /
  `RemoteErrorCode` / `Exception::getKind` infrastructure
  needed for the other 4 symbols (all of which have zero
  recon callers today).  Key reconstruction decisions:
  `ErrorKind` underlying = `uint16_t` (per `cmp $0xa,%di`
  at 0x2e5280); `ZIErrorKind_enum` underlying = `int` (per
  `cmp $0xa,%edi` at 0x2e5240+offset); `ZIErrorKind_enum`
  declared at global scope (matches unqualified
  `16ZIErrorKind_enum` mangling token); `ErrorCondition`
  stand-in is a 16-byte `{int value; int _pad; void const*
  category;}` matching the `rax:rdx` sret layout of
  `make_error_condition`; `singleErrorKindCategory`
  modelled as an opaque `ErrorKindCategoryTag` constexpr
  sentinel in an anonymous namespace (observationally
  equivalent for category-pointer comparison, which is the
  only exposed use; avoids fabricating a full
  `boost::system::error_category` vtable).  Accepted the
  binary's odd `BadRequest → ApiHF2NotSupported (0x801f)`
  and `Timeout → ApiConnectionInvalid (0x800d)` jump-table
  entries (@ 0x962bc0) as-is and tagged `\binarynote`.
  Category name `"zi:kind"` @ 0x90c668; singleton @
  0xb7c5a8.  All 4 mangled symbols
  (`_ZN6zhinst9toApiCodeENS_9ErrorKindE`,
  `_ZN6zhinst20make_error_conditionENS_9ErrorKindE`,
  `_ZN6zhinst13toZiErrorKindENS_9ErrorKindE`,
  `_ZN6zhinst15fromZiErrorKindE16ZIErrorKind_enum`) now
  present in `reconstructed/build/_seqc_compiler.so` (each
  `[1]` in nm).  Truly-absent function count: 97 → 93 (−4).
  1603/1603 main; both builds clean; 0 doxygen warnings
  referencing the new symbols.  Coverage 95.2% → ~94.7%
  (2979/3145) — expected dip from adding new symbols
  faster than briefs; `\binarynote` count 27 → 28.
  Remaining 4 deferred-by-design:
  `isApiError(error_code)` @ 0x2e4490,
  `isApiError(RemoteErrorCode)` @ 0x2e44f0,
  `getApiErrorBase(ZIResult_enum)` @ 0x2e4980,
  `special::toApiCode(Exception)` @ 0x2e7690.

- **2026-05-16** F-followup (bookkeeping): caller scout on
  the 4 still-deferred `api_error_translation::core`
  helpers (commit `887b5b7`, IF-285).  `grep -E
  "call.*\b<addr>\b"` over the full binary disasm: 3 of 4
  have zero callers anywhere; the 4th
  (`isApiError(error_code)` @0x2e4490) has exactly one
  caller, `special::toApiCode(Exception)` @0x2e76ac —
  another deferred member of the same set.  Combined with
  all 4 being `t` (local linkage) and absent from `.dynsym`,
  the group is a self-contained dead island in the original
  `.so`.  Reconstruction would not be exercised by any
  difftest or harness case; deferred indefinitely.  No code
  change; `d14_inventory.md` action item #2 marked
  resolved.

- **2026-05-16** F-followup (bookkeeping): audit of
  `compiler_helpers::codegen` cluster header in
  `d14_inventory.md` found stale prose.  The single member
  `AWGCompilerImpl::nodeListToJson` @0x1088d0 was already
  reconstructed by IF-278 (recon exports the symbol under
  libstdc++ mangling — same qualified name, byte-identical
  `.nodes_json` output, standard libcxx ABI divergence).
  Inventory header described "compileWithRetry (or similar
  — helper not yet identified)" and listed status as
  `absent`, both incorrect.  Per AGENTS.md cluster-prose-is-
  unreliable guidance, refreshed the cluster overview,
  flipped the symbol status to **present (IF-278)**, and
  decremented the truly-absent count.  No source change.
  Truly-absent function count: 93 → 92.

- **2026-05-16** F-followup (bookkeeping): systematic
  exact-mangling audit of all remaining "absent" entries
  in `d14_inventory.md` (IF-286).  Cached recon `nm`,
  cross-checked each of the 47 still-absent entries for
  exact mangled-name matches.  Found **6 stale-absent
  classifications** with zero ABI divergence (recon
  exports the symbol under the binary's exact mangled
  name): `makeDirectories(path)`,
  `runningOnMf64Device()` (nullary), `markFileHidden(path)`,
  `initBoostFilesystemForUnicode()`, `almostEqual(double,
  double)`, `Random::seedRandom()`.  Each cross-referenced
  to its implementing TU via the `@0x<addr>` comment
  convention.  Inventory statuses flipped to **present**;
  truly-absent count: 92 → 86.  No source change.

  Validates the pattern from IF-285 / commit `7d0d0c3`:
  the D14 inventory has drifted out of sync as
  reconstruction has progressed, and per-cluster prose
  cannot be trusted.  Truly-absent count timeline:
  114 (D14 baseline) → 110 → 109 → 97 → 93 → 92 → **86**.

- **2026-05-16** F-followup (bookkeeping): qualified-name
  + body-shape audit of remaining 41 "absent" entries in
  `d14_inventory.md` (IF-287, follow-on to IF-286).
  Demangled the recon symbol list and matched by
  qualified name.  Found 3 distinct outcomes:

  - 2 more **ABI-divergence presents** flipped:
    `runningOnMfDevice(string)` @0x2ec160 and
    `runningOnMf64Device(string)` @0x2ec3d0 — both
    libstdc++ `__cxx11::basic_string` vs binary libc++
    `std::__1::basic_string` mangling; impls at
    `src/io/zi_environment.cpp:160` and `:191`.
  - 4 entries kept absent but **refined as "signature
    mismatch, not ABI divergence"**: the binary's
    `ZIIOException(string, ZIResult_enum)` and
    `ZIAPIException(string, error_code)` two-arg ctors
    (C1 + C2 each) — recon's `ZHINST_DECLARE_EXCEPTION`
    macro emits only `()` and `(string)` overloads.
    These are latent reconstruction opportunities if a
    recon caller ever materialises.
  - 1 stale Status fix: `AwgPathPatterns(AwgPathPatterns
    const&)` @0x2cc4f0 — Notes said "closed (IF-280)"
    but Status still read "absent"; refreshed.

  Truly-absent count: 86 → 83.  Truly-absent timeline:
  114 → 110 → 109 → 97 → 93 → 92 → 86 → **83**.
  Stopping the sweep here — remaining ~38 absents have
  no qualified-name matches in recon and appear to be
  genuine absences.  No source change.

- **2026-05-16** F-followup (bookkeeping): `waveform_misc::waveform`
  cluster class-structure-divergence audit (IF-288).  All 3
  members (`Waveform::File::typeToStr` @0x2a3a90,
  `Waveform::File::typeFromStr` @0x2a63c0,
  `Waveform::File::operator==` @0x2a9680) were already fully
  reconstructed in `src/waveform/waveform.cpp` with verified
  bodies and detailed disassembly commentary; they were
  mis-classified as `absent` only because recon emits them
  under flat class name (`zhinst::WaveformFile::...`) via
  `using Waveform::File = WaveformFile;` whereas the binary
  uses nested class (`zhinst::Waveform::File::...`).  This
  is a third variety of ABI-mangling divergence in the
  inventory, distinct from string-form (IF-287) and
  template-arg (IF-281) divergences.  All three are reachable
  from `Waveform::{toJson,fromJson,operator==}` in both
  binary and recon.  Status flipped to `present
  (class-structure-divergence, IF-288)`; truly-absent
  decremented by 3.  Truly-absent timeline:
  114 → 110 → 109 → 97 → 93 → 92 → 86 → 83 → **80**.
  No source change — promoting `WaveformFile` to a nested
  class would change ABI for every reference for no
  behavioural gain.

- **2026-05-16** F-followup: `diagnostics_text` /
  `zi_environment` / `platform` mass-flip (IF-289).
  Systematic re-audit of the remaining 35 still-"absent"
  entries in `d14_inventory.md` revealed **20 Bucket A
  presents** (libstdc++ `__cxx11::basic_string` vs binary
  libc++ `std::__1::basic_string` mangling, all fully
  reconstructed in `src/core/diagnostics_text.cpp` and
  `src/io/zi_environment.cpp`): `xmlUnescape`,
  `xmlUnescapeCopy`, `entityNumberToTxt`,
  `entityNameToNumber`, `linkToQuery`, `queryToLink`,
  `escapeStringForCsharp`, `replaceUnit`, `browseTo`,
  `sanitizeFilename`, `sanitizeInvalidFilename`,
  `escapeStringForJson`, `escapeStringForPython`,
  `truncateXmlSafe`, `truncateUtf8Safe`,
  `xmlEscapeUtf8Critical`, `xmlEscapeCritical`,
  `generateSfc`, `toCheckedString` (`B5cxx11` ABI tag),
  `hasMediaDevNode`.  Plus 1 **anon-namespace hoist** as a
  real source change: `canCreateFileForWriting` @0x2eb860 was
  wrapped in an anonymous namespace in
  `src/core/platform.cpp`; hoisted to `zhinst::` scope so
  recon emits the binary's exact symbol name.  Largest
  single-audit drop to date.  Truly-absent timeline:
  114 → 110 → 109 → 97 → 93 → 92 → 86 → 83 → 80 → **59**.
  Tests 1603/1603 main + 1626/1626 harness still passing.
  Lesson recorded: when an audit finds the first N matches,
  search exhaustively before declaring diminishing returns
  (IF-289 found ~25% of the supposedly-converged set).

- **2026-05-16** Doc backlog cleanup: removed the phantom
  `StaticResources::errorReportTarget()` declaration from
  `include/zhinst/runtime/resources.hpp` (IF-235 follow-up).
  The declaration had been retained with an `\unclear` marker
  on the justification that it "mirrored the original class
  layout", but re-verification showed that argument doesn't
  hold: the helper was non-virtual (so layout-independent),
  and the binary's `StaticResources` vtable
  (`_ZTVN6zhinst15StaticResourcesE` @0xb03930) contains only
  `D2`/`D0`/`getVariable` — no `errorReportTarget` slot.
  Phantom removed; archaeology of the binary's inline
  `__function::__base::__invoke` dispatch (0x12a256-0x12a26d)
  preserved in IF-235 and a short comment block at the former
  declaration site.  `\unclear` backlog: 1 → 0.  Tests
  1603/1603 main + 1626/1626 harness still passing.

- **2026-05-16** Doc backlog cleanup: cleared the last
  `\verifyme` (IF-290).  `base64::encode` @0x2f8620 used a
  `\verifyme` to flag the implementation choice of recon's
  `std::string::push_back` vs binary's `std::ostringstream`.
  Verified via combined methods: disassembly inspection of
  0x2f8620-0x2f88a0 (confirming `__put_character_sequence`
  emission path, `.rodata` alphabet at 0x90cf90, `"=="`
  padding at 0x90cfd1, and `.str()` SSO-copy epilogue), plus
  exhaustive harness coverage of 23 inputs (empty, three
  pad classes, RFC 4648 vectors, all-bits-set, full-alphabet,
  libc++ SSO boundary, long-string form).  All 23 cases pass
  byte-for-byte.  Promoted `\verifyme` to `\binarynote` with
  an enumerated table of the API-invisible divergences
  (accumulator, per-char emission, padding emission, alphabet
  storage size, allocation profile, exception path).
  `\verifyme` backlog: 1 → 0 (cleared).  `\binarynote`
  count: 28 → 29.  No source-code change.  Tests 1603/1603
  main + 1626/1626 harness still passing.

## Phase G — `\binarynote` re-audit (round 2)

Phase F1 audited the 26 `\binarynote` sites that existed at
2026-05-13 and surfaced IF-269 (`Node::type2str` typo).  Between
F1 close and Phase G start, 8 net-new `\binarynote`s landed via
F2/F3/F4/F5 and the doc-coverage push without binary
re-verification.  Phase G re-audits **all 34 in-tree sites**
(F1 set + the 8 new) on the same methodology — read the body,
disassemble the cited address, log any divergence as an IF,
demote `\binarynote`s where the recon has been silently fixed
since the note was written.

- **G1 (`core/` batch, 11 sites).**  Closed 2026-05-16.  All 9
  actual doc-tag sites verified accurate against the binary
  (spot-checks: `toApiCode` jump-table at @0x2e5280;
  `base64::encode` alphabet @0x90cf90 + pad @0x90cfd1;
  `almostEqual` SSE2 unpcklpd/maxpd/subpd/divpd at @0x2ec070;
  `toRawByteArray` size==0/1 fast paths at @0x2f27c0;
  `sanitizeInvalidFilename` regex literal `COM[1-9]|PRN` at
  @0x90d0b2; `browseTo` no-escape-between-url-and-shell at
  @0x2eb950; `xmlEscapeUtf8Critical` movsbl + `&#%03d;` at
  @0x2faaa0; `Exception::description` returns `&errorCode_` at
  @0x2e58b0; `toSubscript(long)` forwards to string overload at
  @0x2fdb80).  Two of the 11 cited lines turned out to be `//`
  cross-reference comments inside `diagnostics_text.cpp` bodies
  (out of audit scope; don't render).  *Yield*: 0 IFs.  One
  cosmetic fix: `toSubscript(long)`'s `\details` block in
  `core/platform.hpp` contradicted its own sibling `\binarynote`
  ("carry `-` through unchanged" then "drop `-`"); rewritten
  coherently as "forward to string overload, which drops every
  non-digit".  Tests at close: 1603/1603 main.

- **G2 (`device/` + `ast/` + `asm/` batch, 9 sites).**  Closed
  2026-05-16.  All 9 verified accurate against the binary via
  static disasm + rodata cross-checks: `device_type.hpp:146`
  (`None==MF==0` dual emit of `"MFK"` @0x90b60e and `"MF"`
  @0x90b612 via family-conditional `lea` @0x2cfee0);
  `device_type.hpp:1074` lowercase-doesn't-match (already
  re-verified during IF-267); `device_type.hpp:1176` `hasMf`
  family swap (`cmp $0x4,%eax; sete %sil` @0x2d3030);
  `eval_results.hpp:150` `setValue(double)` sub-type `Vect=3`
  (`movl $0x3` @0x2136a0); `seqc_ast_node.cpp:1009` +
  `.hpp:2034` `SeqCValue` partial swap (only `[+0x14]`
  varType + `[+0x18..]` payload @0x1fe410, base fields
  untouched); `seqc_ast_node.hpp:262` `SeqCAstNode` swap
  excludes vptr `[+0x0]` (swaps `[+0x10]` and `[+0x8]`
  @0x1fda40); `seqc_ast_node.hpp:1200` `SeqCRepeat::cond` not
  `count` (only `cond` symbol exists in `.dynsym`);
  `asm_parser_context.hpp:159` `clearSyntaxError` zeroes 4-byte
  flag DWORD + sets `lineNumber_=1` (@0x28e890), leaves
  `doOpt_`/`programCounter_`/`errorCallback_`/`stringCopies_`/
  `labels_` untouched.  *Yield*: 0 IFs, 0 fixes.  Tests at
  close: 1603/1603 main (no source change since G1).

- **G3 (`io/` + `runtime/` + `waveform/` + `codegen/` +
  `infra/` batch, 14 sites).**  Closed 2026-05-16.  13/14
  verified accurate; **1 critical bug surfaced (IF-291)**.

  Verified accurate (12 doc-tags + 1 cross-ref out of scope):
  `io/zi_environment.hpp:149` `makeDirectories` 0x8011 error
  code (@0x2cdef0); `io/cached_parser.hpp:191` `CachedFile`
  field layout (`channel_`/`markerBits_`/`samples_`/`markers_`
  @+0x00/+0x08/+0x20/+0x38; dtor @0x2b1f70 destroys exactly
  3 vectors; sizeof 0x50, no `found` field);
  `io/cached_parser.hpp:279` `cacheFileOutdated` first call
  passes name (@0x2b14f5); `runtime/resources.hpp:659`
  `variableExistsInScope` tail-jump on `grandparent_.get()`
  (@0x1e4390, loads `[r14+0x18]`, never touches `parent_`
  @+0x40); `runtime/resources.hpp:706` `checkVar` accepts
  everything except `VarType_String=3` (`cmpl $0x3,(%rax)`
  @0x1e4e4f, runtime enum not ast enum);
  `waveform/play_config.hpp:46` `now` excluded from
  `operator!=` (only `[+0x1e] dummy` loaded @0x1d57ad);
  `waveform/signal.hpp:192` non-const-ref `Signal::append`
  (mangling confirms; body @0x25f310 calls `checkAllocation`
  on both); `waveform/waveform_generator.hpp:1028,1052`
  `rand`/`randomGauss` 3-arg defaults amplitude=1.0
  (arg-count forks @0x251d27 / @0x252937; rodata 1.0
  @0x956030); `infra/random.hpp:53` `Random::seedRandom`
  call site (@0x149832 uses `__tls_get_addr` result for
  `GlobalResources::random` as `this`); `infra/calver.hpp:88`
  `triple()` returns reference (body @0x100260 is
  `mov %rdi,%rax; ret`); `infra/calver.hpp:248`
  `extractVersionTriple` swallows parse errors (catch
  handler @0x101bb4 zeros all 24 B of sret with
  `xorps`+`movups`+`movq`).  Out of scope: 1 `//` cross-ref
  in `io/zi_environment.cpp:256` (covered by `:149`).

  **IF-291** (`MathCompiler::log` divergence).  The
  `\binarynote` on `MathCompiler::log` claimed the function
  was natural log ("same implementation as `ln`"), and the
  recon body matched that claim (`std::log(x)`).  Binary
  @0x1c3940 actually tail-jumps `log10@plt`, not `log@plt` —
  `log` is **base-10**.  Both sides had drifted in agreement
  while diverging from the binary.  Fixed: recon body now
  `std::log10(x)`; `\binarynote` rewritten to describe the
  base-10 behaviour and to flag it as a surprise relative to
  most ecosystems' natural-log convention.  No `tests/cases/*`
  exercises `log(`, so 1603/1603 stayed clean both before and
  after — exactly the class of bug Phase G is designed to
  catch (recon and doc agreed, binary disagreed, no test
  reached it).

  Tests at close: 1603/1603 main + 1626/1626 harness.

- **G4 (triage + wrap-up).**  Closed 2026-05-16.  Phase G
  audited all 34 in-tree `\binarynote` sites:
  - 33 verified accurate (G1: 11, G2: 9, G3: 13).
  - 1 IF surfaced and fixed (IF-291, G3).
  - 1 cosmetic doc fix (G1: `toSubscript(long)` `\details`
    self-contradiction).
  - 0 demotions / removals — no `\binarynote` had been
    silently superseded.

  Phase G yield rate (1 IF / 34 sites = ~3%) is in line with
  F1 (1 IF / 26 sites).  The recurring pattern across both
  audits is that wrong `\binarynote`s tend to be ones whose
  matching recon body also drifted with them, so the existing
  difftest corpus does not catch them — the audit is the only
  signal.  Suggests Phase H-or-similar should be scheduled
  every time the `\binarynote` count grows by ~10 net-new
  sites without a re-audit.
