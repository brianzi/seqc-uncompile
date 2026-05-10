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
see `notes/symbol-renaming-audit/SYNTHESIS.md` and the
`Symbol Renames (Phase D)` table at the bottom of this document). 226
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
| main (default `manifest.json`) — imports the 6 leaf manifests below | 1600 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-core.json`      | 248 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-zhinst.json`    |  74 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-ziai.json`      | 459 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-ziasm.json`     | 468 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-zivibes.json`   | 259 |
| &nbsp;&nbsp;&nbsp;&nbsp;`manifest-documentation.json` |  92 |
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
phases are fully complete; the corresponding TODO entries are in
[`reconstructed/notes/archive/TODO_phases_13-39.md`](reconstructed/notes/archive/TODO_phases_13-39.md).

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
    └── differential_testing.md      # Test approach, coverage matrix, future development ideas
```

## Open Questions

Active unknowns are tracked in [`reconstructed/notes/unknowns.md`](reconstructed/notes/unknowns.md)
(Closed / Actionable / Blocked / Deferred sections).
Full pre-2026-04-22 history (items 1-116) archived in
[`reconstructed/notes/archive/unknowns_full_1-116.md`](reconstructed/notes/archive/unknowns_full_1-116.md).

Long-deferred items are listed in the **Deferred / Low Priority** section
of `TODO.md`; technical detail for the AsmOptimize trio (simplifyAssign,
splitReg, register-field rename) lives in `notes/optimization_passes.md`.

## Symbol Renames (Phase D)

20 commits (`d15ad32`..`9b2e690`). Score 259/259 throughout (the
size of the diff suite at the time of Phase D, 2026-04-29; the
suite has since grown to 2499/2499).
Per-commit summary:

| Commit | Cluster / Topic | Sites |
|--------|-----------------|-------|
| `d15ad32` c1  | nm-recheck closes audit scan phase | docs only |
| `b857bc3` c2  H | `clone()` → `doClone()` | 19 decls + 17 defs + 59 calls / 4 files |
| `0a993b8` c3  F | `SeqCAstNode::type` param/accessor → `lineNr` | 54 sites / 3 files |
| `8014f3a` c4  G | `first_`/`second_` → role-named in 8 binary AST classes | ~70 sites / 3 files |
| `3e7b911` c5  J | `Waveform`/`WaveformFile` JSON-key field renames | 9 items |
| `a59b4b4` c6+c7 B+A | `isWaveformCmd` → `noOpt` and `flag` → `noOpt` (atomic) | 2 clusters |
| `4346d5a` c8  K | `PlayConfig` producer param renames | producer-side |
| `284b5d1` c9  L | `wvf`/`wvfi` impl param alignment | impl decls |
| `a481fed` c10 D | `channelGrouping` → `loopUnrollLimit` (3-leg) | 3-source rename |
| `c1e3aa3` c11 C | drop Hirzel aliases + `Cache::appendMode_` → `isHirzel_` | aliases + field |
| `26e8b08` c12 E | drop accessor aliases + `PNS::requiredSlots` → `usedCache_` | accessors + field |
| `612eb2a` c13 N | `Resources::parent_` → `grandparent_`, `parentWeak_` swap | 1 class |
| `e22c1b5` c14 §4 | high-confidence singletons | 35 items |
| `5a44521` c15 M | recompose `namespace Assembler` + `AssemblerInstr` → class `Assembler` | recomposition |
| `2477f4e` c16 §2 | open arbitrations | resolved set |
| `da68c0a` c17 | medium-confidence singletons | 31 items |
| `82694d7` c18 | dead-code removals | 2 items |
| `717cf8e` c19 §8/§9 | promote audit findings → `incidental_findings.md` | IF-110..IF-122 |
| `9b2e690` ci  I | `sfc::*Option::Bit0xNNNN` → semantic names (DeviceOption codes) | 52 enumerators / 8 files |

Deferred per audit policy:
- **Phase Q** — 226 low-confidence / unsure cosmetic items
  (stylistic underscore consistency, abbreviation preferences,
  acceptable-but-improvable names). See SYNTHESIS §6.

## Symbol Renames (Phase R) — Audit follow-up

14 commits (`dfc278e`..`2b23826`). Score 259/259 throughout (Phase R
era; the suite has since grown to 2499/2499).
Resolved every deferred arbitration from `2477f4e` and every open
incidental finding promoted in `717cf8e` (IF-110..IF-122).

| Commit | Item | Outcome |
|--------|------|---------|
| `dfc278e` R.0 | IF-111, IF-122 | closed (already fixed in Phase D) |
| `0288bde` IF-121 | dead `DeviceOpts` namespace | removed |
| `dbabd4e` IF-120 | `configFreqSweep` magic literals | wired `kSuserSweep*` constants |
| `49f1463` IF-114 | `PlayConfig::now` | kept (JSON contract) |
| `7a87e7e` IF-118 | `AddressImpl<T>` | kept (single instantiation, ~300 sites) |
| `085eaca` IF-113 | `Cache::Pointer::hash_` | kept |
| `6cee522` Arb 7/9/11 | mergeWaveforms / asm_list / loopArgNodeAppend | kept |
| `43b12c9` IF-115 | polarity-inverted bools | useAbsolute→useMapped fixed; `direction` rename done |
| `43b12c9` IF-116 | `valueType` → `direction` field | type-fix deferred → **FIXED 2026-04-29** |
| `da32249` IF-110/IF-112/IF-119 | trace plans recorded | needs-GDB |
| `69fafbf` IF-110 | `Value::pad_04_` | dismissed (genuine padding, GDB-confirmed) |
| `352ec74` IF-112 + Arb 5 | `NodeMapItem::hasFast` | dismissed (bool correct; GDB on full manifest saw only 0/1) |
| `bf04292` IF-119 | `setPRNGSeed` Var path | **fixed** — latent bug; replaced `AsmRegister(value_.toInt())` with `args[0].reg_` |
| `08c8135` Arb 4 | `Compiler::usedSampleRate_` | **writer reconstructed** at end of `Compiler::compile` (offset `0x1213d6`) |
| `2b23826` Arb 2 | `configFreqSweep` numDIOBits bound | kept (binary-faithful; gate is `kDevSHFPlus`, not UHFLI) |

Outstanding deferrals after Phase R:
- **Phase Q** (226 cosmetic items) — addressed in Phase S below.
- ~~IF-116~~ `EDirection` enum type-fix — **FIXED (2026-04-29)**:
  Converted `int32_t direction` → `EDirection direction` in expression.hpp;
  updated 5 sites in expression.cpp, 13 sites in seqc_parser.tab.c.
  259/259 tests pass.

## Symbol Renames (Phase S) — Phase Q refinement

7 commits (`3f0b24e`..`04e3ac5`). Score 259/259 throughout (Phase S
era; the suite has since grown to 2499/2499).
S.1 reconciled the 226-item backlog into 4 buckets; S.2 landed
mechanical sweeps for Bucket 1.

| Commit | Step | Outcome |
|--------|------|---------|
| `3f0b24e` | TODO | defined Phase S plan |
| `54a1af9` | S.1 | SYNTHESIS §6 reconciliation: B1=15 mech, B2=114 borderline-deferred, B3=39 already-done, B4=58 wontfix |
| `fb40bfb` | S.2 M1 | `AsmCommands::smap` `arg → value` (1 item) |
| `423ec7a` | S.2 M2 | `regInv→regInvalid`, `reg0→regZero` in `play`/`playIndexed` (2 items, 6 sites) |
| `e522deb` | S.2 M3 | `string_218_→pad_218_` in `AWGCompilerImpl` (2 items: 1 rename, 1 kept per audit) |
| `c11ff22` | S.2 M4 | `SeqCNeg::evaluate` local `d → negatedDouble` (1 item) |
| `04e3ac5` | S.2 M5 | dead-param/local removal across asm + custom_functions + awg_assembler (9 items: 8 applied, 1 binary-fidelity skip) |

Phase Q final state: 39 already-done + 58 wontfix + 14 mechanical-resolved
+ 1 binary-fidelity skip = 112 closed; 114 borderline items deferred
case-by-case. No further Phase Q phase planned.

## Phase D — Inline code documentation

In progress.  See `TODO.md` for the phase breakdown (D0..D6) and
`reconstructed/docs/architecture.md` for the rendered Doxygen
mainpage.

**D0 (setup) complete (2026-05-09):**
- Doxygen + Doxygen Awesome CSS configured under `reconstructed/docs/`.
- `cmake --build . --target docs` builds the site to
  `reconstructed/build/docs/html/`.
- Custom aliases `\unclear`, `\verifyme`, `\binarynote` carry the
  accuracy-discipline metadata; each renders to a dedicated
  cross-reference page so the documentation backlog is discoverable.
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

**D4 (per-class public-method briefs) — in progress:**

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
  IF-224, IF-226.  Each is latent — the differential test corpus
  does not exercise the divergent code path, hence 1600/1600
  remains green.
- **Verify-then-write workflow** (AGENTS.md §"Verify-then-write")
  formalised during 2d-2e: every brief opened the function body
  and cross-checked field names against the canonical `.hpp`
  before being written; subagent audits were re-verified before
  IFs were logged.  This caught one false-positive audit claim
  (the audit reported `PlayConfig::now` as misnamed; verification
  showed `now` is the canonical field per `play_config.hpp:47`,
  so no IF was logged).
- 1600/1600 differential tests passing throughout the phase;
  build clean and 0 doxygen warnings at every commit.
