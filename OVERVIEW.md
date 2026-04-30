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

**Differential tests: 259/259 byte-identical** (current as of
2026-04-29, after Phase S wrap-up commit). Score held at 259/259
across every Phase D, R, and S commit.

Phase 24 achievements:
- AWGCompiler pimpl facade (8 bytes → AWGCompilerImpl 0x2C0 bytes)
- AWGCompilerImpl: all 11 public methods reconstructed (compileString,
  compileFile, addWaveforms, writeToStream, writeToFile, writeAssemblerToFile,
  getCompileReport, getJsonWaveformMemoryInfo, setBinVersion,
  setCancelCallback, setProgressCallback)
- compileSeqc() orchestrator: JSON config → DeviceType → AWGCompiler pipeline
- pybind11 entry points: pyCompileSeqc, makeSeqcCompiler, PyInit__seqc_compiler
- ZiFolder utility: DirectoryType enum, folderPath, ziFolder, sessionSaveDirectoryName

**Differential testing: 139/139 test cases pass** (Phase 38, 2026-04-28).
Section-level ELF comparison against the original binary across 10 device
types (HDAWG8, HDAWG4, SHFQA4, SHFQA2, SHFSG8, SHFSG4, SHFSG2, SHFQC,
SHFLI, UHFQA, UHFLI, UHFAWG, GHFLI, VHFLI) covering: arithmetic,
comparison, logical, bitwise, and unary operators; all loop types (for,
while, do-while, repeat); if/else, ternary, switch/case; const/var
declarations; increment/decrement; user-defined functions with return
values; arrays; playZero/playHold/wait/setTrigger/waitWave/
executeTableEntry/startQA/setPRNG/sync builtins; multi-AWG-index; error
and info messages; SHFQC qa/sg sequencer selection; nested control flow;
many-variable register allocation; deep nesting with compound assignments;
full programs with setUserReg; DIO operations; waveform playback and
assignment; dual-channel play; command tables; wait variants across
devices; misc builtins (getDIO, setID, getCnt, etc.); frequency sweep;
ZSync triggers; ternary with runtime variables; extended device coverage
(SHFQA2, SHFSG2/4, SHFLI, UHFAWG, GHFLI, VHFLI).

Phase 38 expanded test coverage from 114→139 cases and fixed 30+ bugs
across 8 categories: parser (VarType_Void), AST evaluation (retType,
switch/case, ternary, array), resources (returnReg_ init, parent→
parentWeak_), custom functions (playZero/playHold dispatch, setRate,
waitTimestamp, getDigTrigger, assignWaveIndex, waitDemod, waitZSync,
setID, suppress mask), waveform/ELF (WaveAssignment copy, double2awg
scale, marker separator, ELF flags, node map entries), prefetch
(prepare/placesingle fixes), ASM commands (wtrig register assignment),
waveform generator (gauss 3-arg), and WavetableFront (waveIndex init).

See `notes/differential_testing.md` for approach, coverage matrix, and
future development ideas.
- Phase 33: Bug-fix session (2026-04-28). 3 fixes, 226/259 tests passing (was ~200).
  **Cache getBestPosition**: appendMode parameter semantics were inverted.
  Binary: `!appendMode` = try-append-at-end fast path (fall back to gap scan via
  recursive call with appendMode=true); `appendMode` = gap-scan. Also added
  empty-pointers early-return (position=0, emplace_back).
  **WavetableIR allocateWaveforms alignment**: Binary aligns `totalSize` (address)
  to `dc->waveformAlignment` before each waveform, but keeps `allocationBytes`
  at 64-byte alignment. Recon had inverted this (aligning allocationBytes to
  waveformAlignment). Fixed: allocationBytes uses `(x+63)&~63`, totalSize uses
  `((x+align-1)/align)*align` with waveformAlignment.
  **prefetch_helpers getMemoryHighWatermark** (prior session): netMemory formula
  corrected to `(addressValue - addressImpl) + memoryBytes`.
- Phase 31a: Quick-win closures (2026-04-26). All 10 items complete.
  Exception prefix string extracted ("ZIException with status code: ");
  sentinel 0x8000 wired into default/string ctors; Compiler+0x18
  confirmed dead; DeviceConstants enums confirmed complete; smap logic
  confirmed complete; checkFunctionSupported comment corrected;
  format<>/SHA-1/WaveIndexTracker all verified correct against binary;
  playIndexed errors 0x98/0x9a wired (arg type + rate type checks).
  5 unknowns closed (#10, #32, #54, #90, #91). 14 open unknowns remain.
- Phase 31b: Node serialization (2026-04-26). Both items complete.
  #27 (opcode==4 skip) verified already correct in AsmList::serialize() Pass 1.
  #28 (#disableOpt) condition fixed: binary appends suffix when
  `isWaveformCmd && opcode ∉ {3,4,5}`, not just opcode==5.
  2 unknowns closed (#27, #28). 12 open unknowns remain.
- Phase 31c: Cache/Prefetch + Signal/Assembler (2026-04-26). 7 of 9 items done.
  #38 fixed (signal numEntries formula: max(1,channels) not channels+1).
  #61 fixed (memoryWrite overlap removal: real erase loop replacing no-op stub).
  #63 fixed (allocate splitting: inverted branch condition).
  #62 verified correct (getBestPosition nameMap), #68 confirmed (UsageEntry=PlayConfig),
  #69 documented (minIndexedSize=4096 for indexed playback), #81 resolved (refactoring only).
  All 9 items done. #45 resolved (register ordering: imm→dst→aux→src→out→label;
  cout message fixed). #75 resolved (both cervino stubs filled: nonsplit emits
  prf with clampToCache; common finalize split per Hirzel/non-Hirzel branch).
  9 unknowns closed (#38, #45, #61, #62, #63, #68, #69, #75, #81).
  **0 open unknowns remain.**
- Phase 31d: Expression/Parser/Boost (2026-04-26). All 5 items complete.
  #93 fixed (pushChild: standard owning shared_ptr, no-op deleter removed).
  #114 fixed (CacheEntry serializes 5 fields not 6, valid_ excluded).
  #55 resolved (SeqcParserContext full 0x38-byte layout: flags+lineNumber+
  padding+std::function; all methods converted to typed members; reset() and
  setErrorCallback() added). Callback dispatch verified (libc++ vtable[6]).
  Flex/bison entries verified (C++ mangled, already correct).
  3 unknowns closed (#55, #93, #114). 0 open unknowns remain.
- Phase 31e: SDK-scope utilities (2026-04-26). Both items complete.
  ZI*Exception helpers (getKind/toApiCode/toZiErrorKind/fromZiErrorKind)
  analyzed and deferred — zero internal callers, SDK API surface only.
  ErrorKind enum (10 values) and ErrorKindCategory documented.
  SeqCAstNode print/clone verified: 53×2 symbols match binary.
  Bug fixed: SeqCVariable::print() tested lineNr_ instead of varType_.
- Phase 31f: Code quality (2026-04-26). All 4 items complete.
  15 placeholder fields renamed: AWGCompilerConfig (unknown_28→serializeRoundTrip,
  string_30→debugDumpPath, string_50→debugJsonPath, unknown_88→optimizationFlags),
  Waveform (field18→formatType, field1C→columnMode, field20→isIntegerFormat).
  reinterpret_cast audited: 102 total, ~80 inherent, 2 eliminated (parserContext_
  retyped from char[0x38] to SeqcParserContext), 11 more eliminable documented.
  28 uncertain comments audited, 3 fixed (resources.cpp mislabel, signal.cpp 1e-12).
  2 open unknowns remain (#45, #75).
  Phase 31 completion: #45 and #75 resolved (see Phase 31c update above).
  **0 open unknowns remain. Phase 31 fully complete.**
- Phase 32: Code quality sweep (2026-04-26). All 7 sub-phases complete.
  32a: Created shared log_macros.hpp and yy_fwd.hpp headers (3 per-file
  copies eliminated). 32b: Named constants for device subtypes, opcode
  groups, DIO/ID addresses, immediate ranges, golden ratio hash, struct
  member offsets. 32c: Variable renames (a→instr 39 sites, c/m→codeVal/msg,
  d→nodeRaw/dict). 32d: ~61 C-style casts converted to static_cast/
  reinterpret_cast. 32e: appendSuser() and sslIndexExceedsPages() helpers
  extracted. 32f: Removed null-cast dead code; assessed bison raw new.
  32g: Wrap-up. Build clean, 28/28 tests pass.
- Phase 33: Backlog sweep (2026-04-26). All 5 sub-phases complete.
  33a: Node serialization #27/#28 verified already implemented.
  33b: appendSuser helper rolled out to 74 call sites across 3 files
  (vector overload added for results->assemblers_ push_back pattern).
  33c: Node +0x4C/+0x60 raw offsets replaced with config.rate and
  config.precompFlags in prefetch_print.cpp (5 reinterpret_casts eliminated).
  33d: 47 SUSER address constants defined in types.hpp; 81+ raw hex
  addresses replaced across 5 source files (custom_functions_play/io/playback,
  asm_commands, seqc_ast_nodes_evaluate). 33e: Print/doClone macro
  verification confirmed done in Phase 31e. Build clean, 28/28 tests pass.
- Phase 34/35: TODO marker cleanup + functional completeness audit (2026-04-26).
  34: Eliminated all actionable TODO markers (20 converted to NOTE/explanatory
  comments), added kAddrTrigger/kAddrInternalTrig constants, replaced 2 more
  reinterpret_casts with config_->isHirzel, implemented compiler.cpp debug
  file write. 35a: Fully reconstructed playIndexed arg-gather loop (non-Aux
  branch @0x161410..0x1615f0), writeToNode Block D all 12 per-case bodies
  (6 fast-path + 6 slow-path) with post-tails, mergeWaveforms dispatch fix,
  configFreqSweep node paths, and 10 other HIGH items. 35b: All 7 MEDIUM
  items completed: randomSeed (mt19937-64 reseed via /dev/urandom),
  Compiler::printAST (debug tree dump), AWGCompiler progress callback
  (setProgress(1.0)), AWGCompiler cancel checks (isCancelled in addWaveforms
  + post-processing), getJsonVersion external_triggering/required_options
  fields, CsvParser cache integration (CachedParser threading + cache-hit
  path). Also: multiply/filter were already complete from prior session.
   Build clean, 28/28 tests pass.
- Phase 34a/34b symbol gap closure complete (2026-04-26). 34a: Implemented
  readManifest (string + cached), doIsMf/isMf/isMf64 (property_tree device
  identity), xmlEscapeSeqToInt (hex/decimal XML codepoint parser) in
  platform.cpp. compressSourceString/checkWaveformInit/getUniqueName confirmed
  already implemented. 34b: Implemented PlayArgs::WaveAssignment variant-aware
  copy ctor in custom_functions.cpp. All other 17 method-level gaps confirmed
  already present. Also fixed: missing forward declarations for WavetableFront,
  WaveformFront, WaveformGenerator, AwgDeviceType in custom_functions.hpp;
  getNodeMapForDevice forward declaration in custom_functions.cpp.
  Build clean, 28/28 tests pass.
- Phase 36 code quality final pass (2026-04-26). 36a: Eliminated all 6
  remaining source markers (3 stale TODO.md cross-refs converted, 3 TBD
  fields resolved: str2_→unusedStr038_, field_168 comment updated to match
  resolved assignedWaveNames_, SeqCValue layout comment updated). Zero
  markers remain in 68.7k lines. 36b: Placeholder field names confirmed
  all resolved (Phase 31f). 36c: Eliminated 11 non-inherent reinterpret_casts
  (82→71): 3 prefetch config_→deviceType, 1 NodeMapItem bool cast, 4
  waveform.cpp SSO hacks, 2 pointer-subtraction byte-size, 1 dead vtable
  read. Remaining 71 confirmed inherent (serialization, tagged-union, TLS,
  ELFIO, zlib, placement-new). Build clean, 28/28 tests pass.
- Phase 25a-c,e: Helper extraction (2026-04-26). All items complete.
  checkExternalTriggeringMode(int) extracted — 10 call sites replaced across
  custom_functions_io.cpp and custom_functions_playback.cpp.
  isShfFamily() extracted — 3 call sites replaced (2 variant sites
  intentionally left unchanged). emitWaitTrigger assessed but not
  extracted — too much variation across sites.
- Phase 28: Binary symbol gap closure (2026-04-26). 451→0 actionable
  missing symbols across 5 sessions. Key fixes: SeqC copy/swap out-of-line
  (+146), AsmRegister global scope (+80), ErrorMessages::format template
  signature fix (inner helper `<T, Args...>` not `<Args...>`, +55),
  AST node type corrections (+12), device factory makeDefault (+14).
  14 non-actionable remain (linker/stdlib artifacts). 28/28 tests pass.
- Phase 29: ZI*Exception hierarchy (2026-04-26). All 26 SDK exception
  subclasses declared and defined via ZHINST_DECLARE/DEFINE_EXCEPTION macros
  in exception.hpp/cpp. Each has default ctor (class-name string), string
  ctor (forwarded to Exception base), defaulted dtor. Helper functions
  getKind/toApiCode deferred (boost error_category plumbing, not compiler-core).
  Builds clean (g++ + clang++/libc++).
- Phase 28: csv_parser.cpp full reconstruction (2026-04-26). Both
  `csvFileToWaveform<WaveformFront>` (@0x2ba8b0) and `<WaveformIR>` (@0x2be830)
  fully reconstructed. Supporting: CsvException (ctor @0x2b8b80, dtor @0x2b8be0),
  setSampleFromString (WaveformFront @0x2b85c0, WaveformIR @0x2b8420),
  getLineVector (@0x2b8c20/@0x2bc200), isCsvSeparator (@0x2ba7d0). Replaces
  throwing stub. Builds clean (g++ + clang++/libc++).
- Phase 27a: AssemblerInstr register field rename — reg0/reg1/reg2 →
  regDst/regAux/regSrc across 10 files (137 references). Comments updated
  to use semantic names. Both builds clean.
- Phase 26a: GetNodeMap data tables extracted (8 devices, 1081 entries).
- Phase 26b: All 6 conservative stubs resolved (oscMaskCheckHirzel,
  oscMaskSetAllHirzel, initNodeMap, secureLoadWaveform, parseOptionalString,
  getPlayRate). Recursive AST printer fully reconstructed. SeqCValue dtor
  memory leak fixed. frontend_lowering.cpp stale stub cleaned up.
- All 41 placeholder field names resolved (6 DeviceConstants, 1
  AWGCompilerConfig, 4 WaveformGenerator, 4 CustomFunctions, 6 Prefetch
  already resolved as cwvfConfig_). Only 3 unknowns remain in
  AWGCompilerConfig (no consumers found).
- 22 of 30 reinterpret_casts in resources.cpp replaced with named field
  access (VarFlag_Written/VarFlag_Frozen, var->value, var->subTypeRaw).
- 18 more reinterpret_casts eliminated in other files (config_->deviceType,
  devConst_->playMinSamples, wf->signal.length_, config_->isHirzel, etc.)
- All 69 magic device-type hex bitmasks replaced with 12 named constants
  (kDevAll, kDevHirzel, kDevSHFPlus, kDevCervino, etc.)
- 2 instances of `0x6e69616d` replaced with `funName == "main"`.
- SeqC AST copy-ctor/operator=/swap for all 53 subclasses via macros +
  per-class implementations. Accessor renames to match binary symbols:
  child()→expr(), function()→funName(), args()→arguments(),
  body()→ifBody() (IfCondition/CondExpr), value()→label() (CaseEntry),
  count()→cond() (Repeat). List node getListElements() + named accessors
  (params/decls/stmts) added.
- All missing toString/str methods implemented: str(EValueCategory),
  str(VarSubType), str(AsmOperationType), toString(AwgSequencerType),
  AssemblerInstr dtor/copy-assign/move-assign, ElfReader getCode/
  getLineMap/getWaveform, WavetableIR alignWaveformSizes/
  assignWaveformAllocationSizes.
- DeviceType extras: 2 factory ctors, deviceType()/toString()/print()/
  swap(), operator==/operator</operator<<, hasOption free function.

### Completed phases (summary)

| Phase | Scope | Key deliverables |
|-------|-------|-----------------|
| 1 | AsmCommands | 83 methods, Cervino/Hirzel impls, factory |
| 2 | Embedded types | Value, Immediate, PlayConfig, Assembler, Node, AsmList, WaveformFront |
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
| 11c | FrontendLowering | FrontendLoweringContext (0x50), FrontendLoweringState (0x38) |
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
| 16a-c | Phase 16 prep | Marker & stub sweep (16a), file-organization split into math_compiler/node_map_data/eval_result_value (16b), TODO.md summary table & unknowns reconciliation (16c) |
| 16d | Stub & gap execution (HIGH+MEDIUM) | **HIGH — CustomFunctions**: ctor binding block 78/81, play() + playIndexed() structurally reconstructed, SubFunc enum resolved (1/2/3/4), 4 complex wrappers identified. 11/13 utility stubs reconstructed (checkFunctionSupported, lookupNode, printF, addWaitCycles, writeLS64bit, etc.). **MEDIUM — WaveformGenerator**: all 32 DSP throw-stubs replaced with real implementations (sin/cos/sinc/ramp/sawtooth/triangle/chirp/drag/blackman/hamming/hann/rrc/mask/marker/rand/randomGauss/randomUniform/lfsrGaloisMarker/vect/placeholder/join/interleave/multiply/cut/flip/filter/circshift/merge/grow). File 772→2305 lines. **MathCompiler**: already complete (23+5+dispatch). Build clean. |
| 16e | Validation against real .so | **Struct size asserts**: added PlayConfig (0x20), Signal (0x58); Cache::Pointer ABI-dependent (0x24 vs 0x28). 14+ existing asserts all pass. **Marker sweep**: 181 total (95 TODO, 29 stub, 22 VERIFY, 14 TBD, 11 APPROXIMATE, 10 not-impl, 0 FIXME) across 40 files. **Comparison tests**: deferred — libc++/libstdc++ ABI mismatch prevents direct linking. |
| 17a | node_map_data depth pass | All 12 method stubs reconstructed from binary (compareEq, hash, clone, getJson for both Virt/DirectAddr; operator==, fastAddress, toJson, size for NodeMapItem). Two rodata lookup tables verified from binary data section: typeTable @0x95ad18 = {2,1,2,2}, sizeTable @0x8fc630 = {2,1,2,2}. **22 markers → 0.** Build clean. |
| 17b | waveform_generator depth pass | **readWave** return type corrected from Signal to shared_ptr\<WaveformFront\> (18 call sites updated). **eval** implemented using EvalResults (make_shared + setValue(VarType(5)) + waveformFront_ assignment). **markerImpl** reconstructed (simpler than expected: 1858B not 4576B, creates uniform Signal with constant marker). **interpolateLinear** bug fixed (size/sizeof removed). Multiple TODO/VERIFY comments resolved via binary verification (grow error=0x3d, merge no length validation, circshift min=1 confirmed, cut offset used directly). **29 markers → 3.** Build clean. |
| 17c | custom_functions depth pass | **11 small methods** reconstructed from binary: checkPlayMinLength (warning 0xF5), checkPlayAlignment (warning 0xE7 + ceiling formula), getWaitTime (left-shift then sar, not right-shift), getNodeAddress (dynamic_cast fast path + at() throw), getSampleClock (fallback to devConst_), getAccessModes (throws out_of_range). **7 0-arg/formatting functions** fully implemented with EvalResults: waitWave (wwvf), waitPlayQueueEmpty (wwvfq), sync (addSyncCommand), randomSeed (seedRandom), now (suser 0x1c), error (printF + asmMessage true), info (printF + asmMessage false). Multiple TODO/TBD comments resolved to NOTEs. **46+9 → 13+6 = 19 markers** (remaining are PlayArgs-blocked stubs + writeToNode 23KB). Build clean. |
| 18b-ii | Medium builtins (6 functions) | **waitDIOTrigger** @0x13d630, **waitZSyncTrigger** @0x13dcf0, **waitCntTrigger** @0x13e460, **getUserReg** @0x14b480, **getSweeperLength** @0x14bca0, **getCnt** @0x14e8d0 — all fully reconstructed with address annotations. Rodata strings verified from binary (AWG_CNT_TRIGGER, AWG_DIG_TRIGGER, AWG_MAP_TRIGGER_INDEX, AWG_ZSYNC_TRIGGER_INDEX, AWG_USERREG_SWEEP_COUNT0/1). DeviceConstants fields confirmed: memoryDepth+0x30 as numUserRegs bound, field_54+0x54 as numCounters, seqClockDivider+0x68 for HDAWG addi. getUserReg HDAWG path includes addSyncCommand when numChannelGroups≥2. **19 → 10 markers.** Build clean. |
| 18b-iii | Large builtins (31→0 stubs) | All 31 one-liner `return nullptr` stubs eliminated. **readConst return type corrected** from void to EvalResultValue (also readString, readWave, readCvar). **37 functions reconstructed** total across 18b-i/ii/iii: wait triggers (waitTrigger, waitAnaTrigger, waitDigTrigger, waitDIOTrigger, waitZSyncTrigger, waitCntTrigger, waitDemodOscPhase, waitSineOscPhase, waitDemodSample, waitQAResultTrigger), oscillator ops (resetOscPhase, setSinePhase, incrementSinePhase, setOscFreq), QA functions (startQA, startQAResult, startQAMonitor, executeTableEntry, getQAResult), PRNG (setPRNGSeed, setPRNGRange, getPRNGValue), feedback (getZSyncData, getFeedback, configureFeedbackProcessing), sweeper (configFreqSweep, setSweepStep, getSweeperLength), register ops (setUserReg, getUserReg, getCnt), triggers (getAnaTrigger, getDigTrigger, at), node I/O (setInt, setDouble — delegate to writeToNode), waveform (assignWaveIndex, generate). **PRNG registers**: 0x74=seed, 0x75=range offset, 0x76=range span, 0x77=value read. File grew from ~2200 to ~5250 lines. Build clean. |
| 19a | EvalResults out-of-line defs | **Created `src/eval_results.cpp` (270 lines).** All 14 methods implemented: ctor(VarType) @0x176bc0, dtor @0x16f3d0, copy ctor @0x231c60, getValue @0x211ab0, 9 setValue overloads (one per call signature: Value, VarType, VarType+int, double, VarType+string, VarType+Value, VarType+Value+int, VarType+VarSubType+Value, VarType+VarSubType+Value+int), addAssembler @0x15c1b0. **All 6 link-blocking symbols now defined**. setValue codegen pattern documented in notes/struct_layouts.md. Build clean. |
| 19b | Resources missing impl (3 blockers) | Appended ~190 lines to `src/resources.cpp` (482→671). **readConst** @0x1e7d70 (partial — int/bool/double variant paths fully decoded; absW≥3 string path TODO), **addConst** @0x1e7010 (full — discovery: writes literal 4 to Variable+0x00 not VarType_Const=3, the on-disk record tag for value-bearing const entries is 4), **newLabel** @0x1ec6b0 (full — uses GlobalResources::labelIndex TLS+0x4c counter via ostringstream). All 3 link-blocker symbols now defined. Error codes documented: 0xb0=UninitializedVar (miss), 0xaf=TypeMismatchWrite, 0xab=AlreadyDefined. Build clean. |
| 19d | Undefined-symbol sweep + triage | **Planning sub-phase only.** Built `nm`-based set difference (U − T) over `libzhinst_seqc.a`: **95 truly-undefined zhinst symbols** out of 303 raw `U` entries (other 208 are inter-TU references resolved within the archive). Authoritative gap list at `/tmp/truly_undefined.txt`; full audit at `notes/linker_resolution.md` (was `undefined_symbols_audit.md`, 420 lines) groups by owning class with addresses, sizes, dependencies, and current callers. **Headline finding**: `ErrorMessages::format<Args...>` accounts for 64/95 symbols (73%); a single header-inline edit clears all of them. Resulting plan: **Phase 20** (5 work-packages 20a–20e), absorbs the Phase 19c carry-forward Resources sweep into 20e. Phase 19c/19d carry-forward investigation items (Variable record-tag mismatch, readConst absW≥3 string path, GlobalResources TLS audit) tracked as 19c-followup prerequisites. |
| 19c-followup | VarType + Variable layout corrections | **All 3 followup investigations resolved.** **(1) VarType enum was wrong**: `str(VarType) @0x247dd0` jump table at 0x95c2a0 yields the correct mapping `Unset=0, Void=1, Var=2, String=3, Const=4, Wave=5, Cvar=6` (was `Unset=0, Var=2, Const=3, Cvar=4, String=5, Wave=6`). Tag mapping confirmed across all add/read overloads — there is **no** separate "record-tag" enum; Variable+0x00 IS the VarType. **(2) Variable layout was wrong**: disasm of readConst @0x1e7d70 (with r12=Variable*) shows `which_` at +0x10 (not +0x08) and variant storage at +0x18 (not +0x10). New layout adds a `subType` (VarSubType) field at +0x08. Size 0x58 confirmed. **(3) TLS layout fully mapped**: 5 zhinst slots in shared `.so` TLS module block — `AsmList::Asm::createUniqueID::nextID` (+0x40), `Node::idCounter_` (+0x44), `GlobalResources::regNumber` (+0x48), `labelIndex` (+0x4c), `random[313]` MT19937-64 state (+0x50, 2504B). Total zhinst TLS = 0x9D8. **Cascading fix applied**: VarType enum + Variable struct rewritten in `resources.hpp`; `str(VarType)`, `toString`, `addConst`, `readConst` (string path filled with new field names) updated in `resources.cpp`; `Variable::~Variable` body filled in; `expression.cpp:177` corrected from `VarType_Const` to `VarType_String` (numeric value 3 unchanged); doc-comments in `eval_results.cpp` and `custom_functions.cpp` re-labeled. **`Node::idCounter_` declaration moved into `Node` class** (was free static `node_id_counter`). `notes/struct_layouts.md` extended with VarType + VarSubType + Variable layout reference tables. Build clean. |
| 20a | Globals + ErrorMessages template body | **WP-A complete: 69/95 truly-undefined symbols resolved (73% gap closure) in one sub-phase.** **(1) ErrorMessages::format<Args...>** body inlined into `error_messages.hpp:457-466` using `boost::format` chained `operator%` driven by `std::initializer_list<int>{...}` (C++17 — safer than fold-expr w.r.t. operator-precedence pitfalls). All 64 distinct template instantiations now generated as weak symbols `W` at the 14 caller TUs. **(2) `errMsg` global** defined in `error_messages.cpp:28` (BSS @ 0x95de60). **(3) `Prefetch::minIndexedSize` static int** defined in `prefetch.cpp:2294` with init value `0x1000` (4096), recovered from `__cxx_global_var_init` at 0xd4361 (`mov DWORD PTR [rip+...],0x1000  # 0xb846d8`). **(4) Three `extern const std::string` globals** (`zsyncDataPqscDecoder`, `zsyncDataPqscRegister`, `constAwgIntegrationTrigger`) defined in `error_messages.cpp:45-47`. String contents recovered from binary rodata via `strings _seqc_compiler.so`: `"ZSYNC_DATA_PQSC_DECODER"`, `"ZSYNC_DATA_PQSC_REGISTER"`, `"AWG_INTEGRATION_TRIGGER"`. Note: binary mangles with `L` prefix (internal linkage); our header declares `extern` for cross-TU access — slight ABI deviation, behaviour unchanged. Build clean. Remaining gap: 26/95 symbols across WP-B through WP-E. |
| 20b | Trivial Rule-of-Five and missing default ctors | **WP-B complete: 8 symbols resolved in one sub-phase. Total now 77/95 (81%).** **(1) `Immediate` copy/move/copy-assign** added to `value.cpp` with per-index switch dispatch matching existing `~Immediate`/`operator int` pattern (placement-new + per-case copy/move; no vtable dispatch — semantically equivalent). **(2) `Value()` default ctor**: `type_=Unspecified, which_=0, storage_.i=0`. Any toX() will throw — matches "uninitialized variable" path. **(3) `Node()` default ctor**: delegates to 3-arg ctor with `(NodeType{0}, 0, -1)`. **(4) `WaveformFile(const char*)` ctor**: copy-constructs `name`, zero-inits POD fields. Important finding: binary's 0x2a7ff0 was a `std::construct_at<>` specialization that inlined the body — no dedicated ctor symbol existed in the binary. **(5) `floatEqual(double,double)` @0x2ec050**: **Surprise** — despite the name, binary body is `cmpeqsd xmm0, xmm1` (exact IEEE-754 bitwise equality), NOT a tolerance check. Callers use it to test for exact zero (e.g. `floatEqual(beta, 0.0)`) which `==` handles identically. **(6) `logExceptionToClog` namespace fix**: `log_exception.cpp` corrected from `zhinst::detail` to `zhinst::logging::detail` to match binary mangling and `logging.cpp` declaration site. All 8 symbols verified `T` in archive. Build clean (1 pre-existing libc++/libstdc++ string-ABI warning, documented). |
| 20c | Wavetable/Waveform ctors and template instantiations | **WP-C complete: 5 symbols resolved (audit listed 6, but `WaveformFile(const char*)` was already cleared in 20b). Total now 82/95 (86%).** **(1) `WaveformIR(string, Type, DC&)` ctor** body recovered from disasm at `0x2aa170-0x2aa20f` (the `allocate_shared<WaveformIR>` dispatcher inlined the ctor — no standalone symbol). Body: copies name+type, zeroes Waveform middle area, `waveIndex=-1`, `minLengthSamples = dc.waveformGranularity (dc+0x40)`, `deviceConstants=&dc`, IR-extension fields all zero, `irField2 = dc.field_24 (dc+0x24)`. **(2) `WaveformFront(string, Type, DC&)` ctor** body recovered from disasm at `0x29b110-0x29b24f` (`newWaveformFromFile` dispatcher). Body identical to IR variant in the Waveform-base region; Front-extension area sets `frontField1 = 1` (notable: IR sets the equivalent byte to 0). **(3) Misleading header comment in `waveform_front.hpp:65` fixed** — the original `"bitsPerSample=dc[0x40]"` was wrong on two counts: dc[0x40] is `waveformGranularity` (not bitsPerSample, which is at +0x50), and the destination field is `minLengthSamples` (+0x70), not bitsPerSample. Replaced with verified offset documentation. **(4) `WaveIndexTracker<WaveformFront/IR>` template ctors** — added explicit-instantiation lines at end of `wave_index_tracker.cpp`; symbols emit as `W` (weak), satisfying the U references at link time. Also fixed pre-existing typo: template body referenced `wp->playIndex` but the field is named `waveIndex` at Waveform+0x6C. **(5) `WavetableManager<WaveformIR>::insertWaveform` specialization** added to `wavetable_manager_ir.cpp` mirroring the WaveformFront body; required a forward-declared specialization at top-of-file because the same TU's earlier ctor and `newWaveform` already implicitly use the function (C++14 [temp.expl.spec]/6). **Note on `nm -u` count**: raw count remains 377 (per-TU `U` references survive in static archives even when `T` exists elsewhere); per-symbol verification with `nm | grep " T \$sym\$"` (or `W`) confirms all 5 targets defined. Build clean. |
| 20d | AWGAssembler/parser/NodeMap small methods | **WP-D complete: 7 symbols resolved in one sub-phase. Total now 89/95 (94%).** **(1) `AWGAssembler::assembleStringToExpressionsVec`** wrapper uncommented in `awg_assembler.cpp:40-47` with correct return type `std::vector<std::shared_ptr<AsmExpression>>`. **(2) `AWGAssemblerImpl::extractComment`** body added at end of `awg_assembler_impl_pipeline.cpp` (find `"//"` substring → return suffix). The binary inlined this twice as the `$_1` lambda inside `assembleStringToExpressionsVec`; we externalize it as a real method to satisfy the cross-TU U reference. **(3) `SeqcParserContext::raiseError` + `setSyntaxError`** implemented in new `src/seqc_parser_context.cpp` using raw byte-offset access (the class is opaque in the header — never instantiated in reconstructed code, only used by-pointer). raiseError uses an indirect callback at `[this+0x30]` (vtable[6]/+0x30 path) with `std::clog` formatted-output fallback; setSyntaxError is a one-byte write at `[this+0x3]`. **ABI risk noted**: the vtable offset is libc++-specific. **(4) `NodeMap::toFrequency(double, double)`** = `(int64_t)(freq * 2^48 / sampleClock)` reinterpreted as uint64_t. Constant `2^48 = 281474976710656.0` from rodata `0x956078 = 0x42f0000000000000`. **(5) `NodeMap::toPhase(float)`** = `roundf(value * (2^23/360))` then 23-bit two's-complement wrap. Scale constant `0x46b60b61 ≈ 23301.689453125f` from rodata `0x8fd2b4` (= 2^23/360). Wrap logic shared with `pauPoffIwrap` (extracted as `wrap23()` helper in anonymous namespace). **(6) `parseOptionalRate`** implemented at end of `custom_functions.cpp`. Disasm-derived semantics: header parameter naming is misleading; the third iterator (rdx) is the *parse cursor* (typically the return of `PlayArgs::parse()`). If exactly one EvalResultValue remains between cursor and `end`, calls `getPlayRate()`. Otherwise encodes the "no-rate" sentinel as `((strict ? 1 : 0) - 1) | 5` (5 if strict, 0xffffffff if not). Throws `CustomFunctionsValueException(format(0xee, name), itemIndex)` if extra unparsed args remain. **Side finding**: libstdc++ vs libc++ ABI mangling — when verifying with `nm | grep " T $sym\$"` using libc++-style mangled names (`__1`, `__wrap_iter`), they DO NOT match, but the static archive is internally consistent — both definitions and per-`.o` U references use the libstdc++ mangling, so they resolve at link time. The clean `cmake --build` (zero warnings, zero unresolved-at-link-time) is the ground truth. Build clean. |
| 20e-i | util::wave + MemoryAllocator + CsvParser cheap wins | **WP-E-i complete: 6 symbols resolved in one sub-phase. Final undefined-zhinst-symbol gap = 0 (all 95 originally-cataloged + the 6 WP-E-i targets now defined; static archive fully self-contained for the zhinst namespace).** **(1) `util::wave::double2awg`** @0x299630 — 14-bit signed sample with 2 marker bits in low. Constants from rodata: 1.0 (`0x956030`), -1.0 (`0x956068`), 8190.0 (`0x956330`). Saturate path: if sample > 1.0, use 8190 directly; else `max(-1.0, sample) * 8190`, then `lround`. Pack: `(rounded << 2) \| (marker & 0x3)`. **(2) `util::wave::double2awg1m`** @0x299680 — 15-bit signed sample with 1 marker bit. Scale 16383.0 (`0x956338`). **(3) `util::wave::double2awg16`** @0x299700 — 16-bit signed sample, no marker. Scale 32767.0 (`0x956340`). **(4) `util::wave::hash`** @0x299760 — **uses `boost::uuids::detail::sha1`** internally (binary calls `process_block` @0x29a270 and `get_digest` @0x29a000). Returns `vector<uint32>` of 5 words (160-bit SHA-1, MSB-first per word). Reconstructed via `boost::uuids::detail::sha1::process_bytes` + `get_digest(uchar[20])` with manual MSB-first packing into uint32 to match binary's bswap-then-store sequence. **Resolves open question #1**: the IV constants at `0x8fc720` (`67452301 efcdab89 98badcfe 10325476 c3d2e1f0`) are the standard SHA-1 initial state; archived Phase 12c 'no CsvParser symbols' note was wrong on a related front (see #6 below). **(5) `MemoryAllocator(const DeviceConstants*, uint32_t)` ctor** — inlined at all binary call sites (no standalone symbol), but the build host emits an unresolved reference from any TU that takes the ctor's address. Provided real ctor matching documented 0x70-byte layout: copies `dc` and `startOffset`, sets `lastAllocEnd_=0xFFFFFFFF`, derives `memorySizeInSamples_/cacheLineSize_/maxBlocksPerCL_` from `dc->{waveformMemorySize,waveformAlignment,cachePageCount}`, initializes per-CL ownership table to all `0xFFFFFFFF` (free), `numCacheLines_ = memorySize / cacheLineSize` (with cacheLineSize=0 guard for non-cached families). **(6) `CsvParser::csvFileToWaveform<WaveformIR>`** @0x2be830 — STUB. Full reconstruction deferred (~7000-byte function with `0x528` stack frame, complex CachedParser/CSV-parsing pipeline, not on hot path for non-CSV SeqC programs). Stub throws `std::runtime_error` so callers fail loudly rather than silently producing zero-filled waveforms. **Resolves open question #1** definitively: binary contains `csvFileToWaveform<WaveformFront>` @0x2ba8b0 AND `<WaveformIR>` @0x2be830 as full template specializations (not inlined as Phase 12c claimed). New TUs: `src/util_wave.cpp`, `src/csv_parser.cpp`. Build clean, no warnings. |
| 21a | custom_functions throw-stub closure (5 wrappers) | All 5 throw-stubs in `custom_functions.cpp` reconstructed across 5 sub-batches. **(1) playWaveDIO** @0x137740 (187 disasm lines) — externalTriggeringMode_ check (1→OK), `args.empty()` → error 0x42, `wvft(reg, 1<<numOutputPorts)` direct emission. **(2) playWaveZSync** @0x137a50 (697 disasm lines) — externalTriggeringMode_ check (state=2, distinct from DIO's 1), `args.size()==1` strict, single Const matched against `ZSYNC_DATA_RAW`/`PROCESSED_A`/`PROCESSED_B` → bit-shifts `1/9/0xd`, mask formula `shift << numOutputPorts` (multi-bit pattern preserved). **(3) mergeWaveforms** @0x15e060 (893 disasm → 288 C++ lines) — header signature corrected `int64_t→bool` for last param (mangling `...sbRK<string>ib`); 7-phase body (empty/extract/validate/check/name/factory/construct); error 0x9E format `(string, short, ushort)` confirms `Js t` mangling tail; **two pre-existing call-site bugs fixed** in `play` and `assignWaveIndex`. **(4) playDIOWave** @0x1369f0 (819 disasm → 229 C++ lines) — `PlayArgs(indexed=false)`, `parseOptionalRate(strict=false)`, rate guard error 0xa1 (≤1), per-bit trigger mask clearing `mask &= ~(0x40 << (7*b))`, `WaveAssignment::bits` at +0x38 confirmed. **(5) playAuxWave** @0x135610 (1118 disasm → 242 C++ lines) — no externalTriggeringMode_ check (distinct from DIO/ZSync), `PlayArgs(indexed=true)`, `parseOptionalRate(strict=true)`, rate guard error 0xa0 (≤4), channel scatter `wa.value→channelArgs[bits[i]-1]`, zero-fill via `waveformGen_->call("zeros",{Value(len)})`, constant trigger mask, `asmPlay isHoldMode=true`. **WaveAssignment layout corrected**: 0x58→0x50 (historical "type"/"subType" fields were just `value.varType_`/`value.varSubType_`); 8 call sites updated. Cumulative: ~3700 disasm lines → ~1100 lines annotated C++. Subagent dispatch pattern validated for the 3 large reconstructions (mergeWaveforms/playDIOWave/playAuxWave). Build clean. See `notes/struct_layouts.md` (PlayArgs::WaveAssignment, play wrapper signature matrix, mergeWaveforms 6-arg semantics). |

| 21a-followup | Surgical TODOs from 21a (6 items) | All 6 surgical TODOs surfaced during Phase 21a reconstruction cleared. **(1) WaveformFront channels accessor** — replaced `reinterpret_cast<char const*>(wf) + 0xC8` raw read in mergeWaveforms phase 7 with `wf->signal.channels()` (`Signal::channels()` already exists at `signal.hpp:83`; `signal+0x48 = WaveformFront+0xC8`). **(2) WaveformFront +0x48 field** — `assignWaveIndex` `@0x1342f1` `mov BYTE PTR [rax+0x48], 0x1` is `Waveform::used = true` (already named in `waveform.hpp:104`); replaced field-offset comment with proper field access. **(3) playAuxWave config field +0x16** — `@0x135889` `movzx WORD PTR [rax+0x16]` decoded as `config_->channelsPerGroup[1]` (the INDEXED variant; playAuxWave uses indexed=true so picks slot [1] of the 2-element uint16 array at config+0x14). **(4-5) mergeWaveforms call-site placeholders** — both call sites (`play` `@0x15fa75`, `assignWaveIndex` `@0x13424c`) decoded: `param5` is `(int)PlayArgs::getMaxSampleLength()` truncated from int64 to int per SysV stack-arg slot semantics (set @0x15f634 / @0x13400a after `PlayArgs::getMaxSampleLength@0x15f62f` / `@0x133fce`); `param6` in `play` is `(ch != referenceChannelIndex)` from `setne al; cmp r14,[rbp-0xd8]` where `[rbp-0xd8] = r13[+0x24]`; `param6` in `assignWaveIndex` is hardcoded `false` (literal `push 0x0`). **(6) mergeWaveforms factory selection** — three factory targets confirmed by direct `lea rax,[rip+...]` loads: `interleave@0x258140`, `merge@0x25f5c0`, `grow@0x260640`. Multi-value branch dispatch `test bl,bl; je` at `@0x15e774` reads `bl` from `[rbp-0x48]` (a function-local value derived in earlier multi-value handling, not a direct incoming parameter); single-value branch is unconditionally GROW. Source approximation `(multiValue, useYSuffix)→{interleave,merge,grow}` documented as approximate; exact `[rbp-0x48]` derivation deferred. **`custom_functions.cpp` marker count 45→35** (10 markers cleared). Build clean. |
| 21b-prereq-A | Audit unaccounted mergeWaveforms call sites | Original framing assumed 5 binary call sites vs 2 source. Audit found we actually had 4/5 implemented — sites @0x135ddc and @0x136cfa were already in `playAuxWave` (line 1809) and `playDIOWave` (line 2083). Only @0x161c2b was missing, and it lives inside `playIndexed` @0x160e00 (a 6428-byte function currently stubbed as ~64 lines at `custom_functions.cpp:1219-1282`). Reclassified as full reconstruction work → spawned **21b-prereq-B** for `playIndexed`. Notes updated with full call-site map. No source changes; <1 session. |
| 21b-prereq-B | Reconstruct `playIndexed` @0x160e00 (6428 bytes) — all 18 phases | Structural map of 18 phases derived from disasm landmarks, all phases now executable C++. **Phases 1-5** with three CRITICAL bug fixes vs prior skeleton: arg-count guard `<2`→`<3` (binary divides by 56 then `>2`), `indexed` flag inverted (`!=Aux`→`==Aux`), and removed wrong `asmTable` path (binary uses `addi+asmPlay` pattern at @0x161e56+@0x162343). **Phase 5b added** — `waveIndex==0` does NOT throw; it formats error 0x9c, calls `warningCallback_` via vtable[+0x30] indirect through `[res+0x1b0]`, then returns empty results. **Phases 6-7** scaffolded with `regZero`, `channelArgs` vector, `triggerMask=0x3fff` and Aux-vs-non-Aux split. **Phases 8-18 reconstructed** in second sub-phase pass: getWaveformSampleLength probe @0x161853, `createDummyWaveform(baseLen)` @0x161951 (the documented WaveformGenerator helper for the inline "zeros" idiom), mergeWaveforms call @0x161c2b (the 5th binary call site, now in source), loadWaveform @0x161d76, getRegisterNumber+addi+asmSetVarPlaceholder @0x161df1+0x161e56+0x161ee2, checkOffspecWaveLength @0x16214a, asmPlay @0x162343 (12-arg call, modeled after play() pattern), results->assemblers_.push_back @0x162511, and a documented summary of the error tail @0x162848+ (errors 0x3d, 0x98, 0xa0, 0x9a wired into Phase 2/4/5b/12 throws). Residual TODO(21b-followup-3) markers at: per-channel WaveAssignment vector access for the getWaveformSampleLength probe (Phase 8); precise channelsPerGroup index selection for mergeWaveforms (Phase 10); waveIndex bounds check against `combined->field_d0_` before loadWaveform (Phase 11); local-Assembler push tracking for addi/placeholder entries (Phase 14); precise `config_->[+0x40]` field name for checkOffspecWaveLength (Phase 15); and full SysV mapping of asmPlay's 12 args (Phase 16). Build clean. |
| 21b.1 | writeToNode skeleton + Block A (absDevRegex) | Function recon: 0x71f0 bytes (29KB, 6120 disasm lines) — bigger than the historical 23KB estimate. **Return type fix**: declaration changed from `void` to `std::shared_ptr<EvalResults>` (binary uses sret slot rdi, stores control block + payload at `[rbx]`/`[rbx+8]`). **Setup** (~180B): `make_shared<EvalResults>()` matches the binary's single 0x98-byte `__shared_ptr_emplace<EvalResults>` allocation (vtable @b03d00) with zero-initialised body; `path.varType_ == 2` (Var) precondition jumps to error tail @0x169df4 on failure. **Block A** (~500B): `boost::regex_match` against `absDevRegex` static; on match, parse numeric capture via `stoul` and validate against `config_->deviceIndex` (= `[this+0x00]→[+0x24]`, the Phase 21b-followup-1 finding); replace working path string with second capture (rest-of-path) via libc++ short-string xmm copy. **Trailing `lookupNode(pathStr)`** call @0x1647ed performed unconditionally after Block A. 4 static `boost::regex` placeholders declared in anon namespace (real pattern strings recovered in 21b.5 from .rodata referenced at cold-path constructors @0x169ea5..). Blocks B/C/D/E/F/G left as explicit TODO sub-phase markers. setInt's existing `writeToNode(...); return nullptr;` left in place with TODO 21b.5 marker. Build clean. See `notes/struct_layouts.md` (writeToNode recon section). |
| 21b.2 | writeToNode Blocks B + C (awgNodeRegex + sineNodeRegex) | **Block B** (awgNodeRegex, ~290B @0x164803..16491e): match `awgs/<channel>/...` style relative path; extract numeric capture via `sub_match::str()` + `stoul`; normalise channel index against `config_->numChannelGroups` ([config+0x1c], values 1/2/4) — `numGroups==2 → idx/=2` (signed shift @0x1648f9..164904), `numGroups==4 → idx/=4` (signed shift via lea+cmovns+sar @0x1649e1..1649ef), else unchanged; validate `idx == config_->unknown_20` ([config+0x20]) → error @0x1649ff on mismatch (formats with `[config+0x24]` device-id). **Block C** (sineNodeRegex, ~460B @0x16491e..164ae4): match `sines/<oscIdx>/...`; same extract/parse pattern; map oscillator → channel index via {2,4,8} oscs-per-channel logic (`numGroups==2 ? /=4 : /=2`, then if `numGroups==4` also `/=4` for total `/=8`) — encoded via `xor/sete/lea esi,[rax*2+0x2]; idiv esi` @0x164ab4..164ac8 plus secondary `cmp r12d,0x4 ... sar edx,0x2`; same channel-cap validation. Both blocks share the error tail at @0x1649ff/@0x16a006. Block B/C error message strings TODO 21b.5. Build clean. See `notes/struct_layouts.md` (writeToNode Blocks B+C section). |
| 21b.3 | writeToNode Block D structural skeleton (oscselNodeRegex, 21KB) | **Stack-frame discovery**: `[rbp-0x1c0]..[rbp-0x180]` is the local 64-byte `NodeMapItem` returned by `lookupNode(pathStr)`; `[rbp-0x1b8] = node.typeIdx` (+0x08 dword) and `[rbp-0x1b0] = node.hasFast` (+0x10 byte) — both match the existing `node_map_data.hpp:93+` layout. The cleanup at @0x16b6db (virtual dtor call on raw `NodeMapData*`) confirms `data` is a raw pointer, not a shared_ptr. **Block D structural flow** (~21KB total): regex match → insert `"MF"` (libc++ SSO short-string with size-byte 4) into `usedFeatures_` (set<string> at +0x1C8; tag purpose TODO 21b.5) → `addNodeAccess(node, accessMode)` → `getRegisterNumber()` + `AsmRegister destReg` + local `AsmList` accumulator → branch on `node.hasFast`: fast-path uses `node.fastAddress()` (typeIdx jump table @958f68), slow-path uses `nodeAddressMap_.find(node)` from `+0x100 unordered_map` (typeIdx jump table @958f50). Both jump tables have 6 cases (typeIdx 0..5); cumulatively Block D contains 53 `suser` + 44 `addi` + 25 `AsmList::append` calls. **One representative case implemented as exemplar**: fast-path case 2 emits `asmCommands_->suser(val.reg_, AddressImpl(0x17+addr))` after `val.varType_==2` precondition (matches @0x164c7f..164cb4). All other 11 cases stubbed as explicit TODO 21b.4 markers with disasm-address pointers. `asmCommands_` confirmed at `[this+0x50]` (raw ptr from `shared_ptr<AsmCommands>`). Default-fall-through to "no regex matched" error at @0x169d83 also wired (TODO 21b.5 for exact code/string). Build clean. See `notes/struct_layouts.md` (writeToNode Block D structural skeleton section). |
| 21b.3-fix | writeToNode Block D structural correction | Re-examined Block D dispatch and found the 21b.3 2-way (fast/slow) model was incomplete. Actual structure is **3-way**: (A) `hasFast == true` @0x164c50..164c5c → `addr = node.fastAddress()` + jt @958f68 (cases 0..5); (B) `hasFast == false` AND `dynamic_cast<DirectAddrNodeMapData*>(node.data)` succeeds @0x164cb9..164cdf → `addr = direct->addr_` + jt @958f50; (C) `hasFast == false` AND dyncast fails (or `data == nullptr`) @0x164d92..164da8 → `nodeAddressMap_.find(node)` + jt @958f50. Paths (B) and (C) **share** the slow-path jump table @958f50; path (A) uses jt @958f68. The `dynamic_cast` was missed in 21b.3 because both paths converge at @0x164dcc. Also: `usedFeatures_.insert("MF")` is **unconditional** (was incorrectly commented as unverified — binary always runs the insert at @0x164b51..164bee). Added the `typeIdx > 5` → @0x164d05 boost::log warning early-return path (writes severity-1 log record with the unsupported typeIdx, then early-returns based on `config_->something==2`). Source flattened to a single `useFastJt` flag + `addr` value to mirror the binary's structure cleanly. The slow-path case-0 disasm @0x164de4..164e58 also confirmed: `addi(asmCommands_, destReg, AsmRegister(0), Immediate(1))` returns a vector that's spliced into `localList` via `vector::__insert_with_size` — the `addi` member returns a `vector<Asm>` (multiple instructions), not a single Asm. Build clean. |
| 21b.4-map | Block D chained-emit protocol catalogue | Discovered during attempted per-case implementation that the "11 case bodies" model from 21b.3 was misleading — each case is a chained sequence of `(case-compute → suser-opcode-0x10 → addi-load-addr)` emit-step blocks, with 34 such blocks total across the 12 cases. **Did NOT change source** in this sub-phase; produced a 253-line catalogue at `notes/writeToNode_block_d_protocol.md` instead. Key findings: (1) **3 case shapes** — S1 single-step (cases A.{1,2,3,5} + BC.{0,1,2,3}), S2 direct passthrough (A.{0,4}), S3 low/mid/high triplet (BC.{4,5}). (2) **suser opcode set**: 0x10 (low/set, 11×), 0x11 (mid/addr-tag, 6×), 0x12 (high/commit, 6×), 0x17 (direct, 9×), 0x19 (direct-B, 4×). The triplet shape implements multi-word user-store: load tag at 0x10, load addr at 0x11, commit data at 0x12. (3) **Immediate sources**: literal small ints (1, 2, 3, 0xc, 0xd), resolved `addr`, `NodeMap::toFrequency(toDouble, getSampleClock)`, `NodeMap::toPhase(toFloat)`, raw float-bits via cvtsd2ss, and `val.toInt()` for the variant-dispatch slow-arm. (4) **Variant-dispatch slow arms**: every path-A case begins `cmp [r12],0x2; jne <slow>` — fast arm assumes `val.varType_ == 2` (already-AsmRegister); slow arm runs `boost::apply_visitor` via vtable @b03dc0 to extract value, calls `floatEqual(val,toInt(val))` to validate, throws `CustomFunctionsValueException` (ErrorMessage 0x80) on non-integer. Path-BC skips the precheck because `dynamic_cast<DirectAddrNodeMapData*>` upstream guaranteed the variant is resolved. (5) **Stack-frame layout**: per-step Immediate sret slots walk down the frame by 0x20..0x60 bytes per step (0x8d0, 0x8b0, 0x6b0, ...), explaining the 0xad8-byte frame size. (6) **Common-tail blocks**: 10 distinct tail labels (e.g. 0x165c90, 0x166a3e, 0x1678e2) finalise localList and splice into results. **Open questions** flagged for 21b.4-impl: dump jt-bytes at 0x958f50/0x958f68 to verify case order; confirm 0x10/0x11/0x12 ↔ existing user_store_{lo,mid,hi} opcodes in `opcode_map.md`; document boost::variant @b03dc0 visitor layout. No source changes; build necessarily clean. |
| 21b.4-impl | Block D per-case asm-emission bodies (12 cases) | **Jump-table order verified** by dumping .rodata at @0x958f50/@0x958f68 — the catalogue's linear-order assumption was wrong for several cases. Corrected mapping: Path A {0→0x164c7f register-passthrough, 1→0x165263 sine-pair, 2→0x165013 float-bits, 3→0x165137 raw-double-bits, 4→0x164ee7 frequency, 5→0x1652e6 phase}; Path BC {0→0x164de4 Imm(1), 1→0x16591b Imm(2), 2→0x165587 double-triplet, 3→0x165751 single-triplet-Q, 4→0x165488 single-triplet, 5→0x165a17 Imm(3)}. **All 12 case bodies implemented** in source: (A.0) `suser(val.reg_, 0x17)` + append; (A.1) `suser(val.reg_, 0x17)` + `suser(R0, 0x19)` [sine pair]; (A.2-A.5) compute Immediate via toDouble/cvtsd2ss/toFrequency/toPhase → `addi(destReg, R0, Imm)` + `suser(destReg, 0x17)`; (BC.0,1,5) `addi(destReg, R0, Imm(1/2/3))` → `suser(destReg, 0x10)` → `addi(destReg, R0, Imm(addr))`; (BC.2) double-triplet with tags 0xc/0xd × {suser(0x10), suser(0x11), suser(0x12)}; (BC.3,4) single triplets. **Key correction from 21b.3**: A.0's suser uses literal 0x17, NOT `0x17+addr` — addr goes into the common-tail addi. **varType polarity**: A.0/A.1 require varType==2 (register direct); A.2-A.5 require varType!=2 (scalar); BC.2-BC.5 have varType checks with slow-arm stubs (TODO 21b.5). Common tails: path A always ends with `addi(destReg, R0, Imm(addr))`; path BC cases 0,1,5 end with `suser(destReg, 0x10) + addi(destReg, R0, Imm(addr))`; BC cases 2,3,4 embed suser sequences inline. localList splice into results→assemblers_ placeholder added (verification TODO 21b.5). Added `floatEqual` forward-decl + `#include <cstring>`. Build clean. |
| 21b.5 | writeToNode slow-arms + splice + setInt/setDouble + epilogue | **All 10 slow-arm stubs resolved.** 6 are simple throws with binary-verified error IDs: A.2/A.3 throw 0x7f, A.4 throw 0x81, A.5 throw 0x82, BC.4 throw 0x7f, BC.5 throw 0x82 — all via `ErrorMessages::format(id, val.toString())`. 4 are real codegen: **A.0** (`toDouble→toInt→floatEqual` guard + warning 0x80 + `addi(Imm(intVal))+suser(0x17)`), **A.1** (split raw double into low32/high32 + `addi+suser(0x17)` + `addi+suser(0x19)` with floatEqual warning), **BC.2** (tag 3 + float-bits via `cvtsd2ss` + second-value floatEqual check), **BC.3** (tag 4 + 64-bit double split low32/high32 via `suser(0x12)+suser(0x13)` + second-value float check). **localList splice** verified: `results->assemblers_` at EvalResults+0x18. **setInt/setDouble** now return `writeToNode(...)` result. **Block E** reclassified: 0x169d83 is cleanup (shared_ptr release), not error; 0x169df4 is the normal `ret` epilogue; error 0x84 at 0x169e0d is a cold-path throw from Block D's oscsel iteration. **Block F** (regex lazy-init) cancelled — cold-path ctor guards, not user-visible. **New discovery**: path-A cases have per-case post-tails with additional suser(0x16/0x11/0x19) emissions beyond the modeled common tail — 7 distinct tail sequences exist. Cases 0,3,5 share a simple `suser(0x16)` tail; cases 1,2,4 have extended tails (case 1: `suser(0x11)+toFrequency+addi`; case 2: `suser(0x11)+toPhase+addi`; case 4: `suser(0x19)+addi(r13d)+suser(0x16)`). These tails are NOT yet implemented — documented as new TODO item 21b-followup-4. Build clean. |
| 21c | AsmOptimize trio completion (splitReg + simplifyAssign + header fix) | **splitReg @0x281000** fully reconstructed (was 20-line stub → now ~100 lines). Binary algorithm is a live-range splitter with ≥10-instruction threshold: scans instructions in `(start, end]`, counts reads of `reg` (using getCmdType bitmask, skipping INVALID/LABEL/cmd4 via 0x29 bitmask), allocates fresh virtual register from `GlobalResources::regNumber` TLS, overwrites boundary instruction slots at `start` and `end` with ADDI copy instructions (`newReg = reg + 0`), replaces `reg0`/`reg1` references with `newReg`, then kills originals if all splits succeeded. **simplifyAssign @0x280e10** corrected — 4 bugs fixed: (1) immediate check was `immediates.back()`, binary checks `outputs[0]` (`+0x38` vector, not `+0x08`); (2) register comparison was `reg1`/`reg2`, binary checks `next->reg2 == it->reg0` (read-src of ADDI == write-dest of current); (3) scan loop was from `next+1`, binary starts at `it+2`; (4) simplification stored to `reg1`, binary stores to `reg0`. The algorithm is copy-propagation: when an instruction writes to `rX` followed by `ADDI rY, rX, 0` (copy), and rX is never read again, redirect the write to rY directly and kill the ADDI. **assembler.hpp** register-semantic comments corrected: `reg2` (+0x20) = READ source, `reg0` (+0x28) = WRITE dest, `reg1` (+0x30) = dual-role. Phase 1's original `reg2=dest/reg0=src` labels were backwards. Field rename deferred (20+ file cascade). Build clean. |
| 21d | SeqCAstNode evaluate() virtual + VarType field correction | **Vtable layout fully verified from binary vtable dumps.** Corrected order: vptr[0]=evaluate(3-arg), vptr[1]=getListElements, vptr[2]=children, vptr[3]=print, vptr[4]=clone, vptr[5-6]=D2/D0 dtors, vptr[7]=getVarTypes. Prior header had dtors at slots 2-3 and evaluate at slot 8 — completely wrong. **evaluate() signature decoded from `nm -C`**: 3-arg `virtual shared_ptr<EvalResults> evaluate(shared_ptr<Resources>, FrontendLoweringContext&, FrontendLoweringState&) const` — base @0x209dc0 returns null shared_ptr. Binary TU: `SeqCAstNodesEvaluate.cpp`. **SeqCOperator adds vptr[8]**: 5-arg `evaluate(…, EvalResults const&, EvalResults const&)` — template-method pattern where 3-arg dispatches to 5-arg after evaluating children. **VarType at +0x14 is NOT padding** — every derived ctor takes `(EValueCategory, int, EDirection, VarType, …children…)` as confirmed by SeqCParamList ctor @0x200cc0 `mov [rdi+0x14], r8d`. All 53 subclass ctors updated to accept VarType as 4th explicit parameter. **SeqCParameter class REMOVED** — `nm -C` shows zero SeqCParameter symbols; the class was our placeholder. VarType is now a proper base-class field with `varType()` accessor. `addArguments(SeqCAstNode const&)` updated to use `node.varType()` and `static_cast<SeqCVariable const*>(node)->name()`. **getVarTypes() at vptr[7]** — base impl @0x1fdb40 returns `{str(varType_)}`; SeqCParamList override @0x200f20 iterates children. Unknowns #119 resolved. Build clean. |
| 21f | Small markers sweep + AwgDeviceProps field verification | **8 items resolved: (1)** `readDoubleAmplitude` @0x25caa0: fabs>1.0 warning via `warningCallback_` (error 0x54, not a throw). **(2)** `lfsrGaloisMarker` marker validation: valid={1,2}, unsigned range-check idiom `(marker-3)<=0xfffffffd`, error 0x64. **(3)** Interpolation formula confirmed correct. **(4) AwgDeviceProps fields VERIFIED from consumers**: `maxWaveformSamples`→`maxElfSize` (JSON `"maxelfsize"`); `maxWaveformBytes`→split into `addressImpl` (uint32 low) + `sampleFormat` (uint32 high) — explains HDAWG's 0x180000000; `supportsExtraFeature`→`isHirzel`. 9 instantiations + header updated. **(5)** `CachedParser::cacheFile` @0x2b05b0 fully reconstructed (~880 insns): file ext `.wave` not `.elf`; `csv<hash2str>.wave`; machineType=3; section↔param mapping corrected. **(6)** `removeOldFiles` `void`→`bool`; cacheFile bails on pinned. **(7)** `compiler.cpp` evaluate() wired up (unblocked by 21d). **(8)** SeqCOperator comment fix. **93 TODOs remain** (65 in custom_functions.cpp). Build clean. |
| 21g | libc++/libstdc++ ABI dual-build strategy | **Design + validation sub-phase.** Surveyed all ABI workarounds: ~45 reinterpret_cast, ~15 placement-new, ~25 static_asserts, 2 GCC pragmas, 0 conditional compilation. Evaluated 3 strategies: (a) clang+libc++ dual-build — **adopted**; (b) translation header — rejected (too complex); (c) accept libstdc++ as canonical — retained as primary build. **Implemented**: `build-libcxx/` directory with `clang++ -stdlib=libc++`, ABI-adaptive static_asserts (`N * sizeof(std::string)`), conditional `std::function` size assert. **Result**: both builds clean (g++: 0 errors 1 warning; clang++: 0 errors 6 warnings — all known/benign). Decision + full workaround inventory documented in `notes/libcpp_abi.md`. |
| 21h | SeqCAstNode evaluate() body reconstruction (9 sub-phases) | **21h.1-21h.3**: `combine(VarType,VarType)` 7×7 lookup table, `VarTypeException` class, `SeqCOperator::evaluate(3-arg)` template-method, `SeqCValue::evaluate` (double/string paths), `SeqCVariable::evaluate` @0x209ea0 (3712B, 7-way + 5-way cascaded dispatch tables), `SeqCAssign::evaluate` @0x243e60 (5552B, 13 action rows, sret+this ABI puzzle). **21h.4-21h.7**: All 4 arithmetic operators — SeqCPlus @0x22a600 (8-row), SeqCMinus @0x22cde0 (7-row), SeqCMult @0x22ea70 (6-row + computeMult shift-and-add), SeqCDiv @0x231070 (5-row + floatEqual divide-by-zero). **21h.8**: 5 anonymous-namespace helper bodies (scaleWaveform×2, constWaveform, combineWaveforms, computeMult ~3000B shift-and-add loop). **21h.9**: `combine(VarSubType,VarSubType)` @0x247ea0 (pure conditional logic, 60 bytes). **21h.3-followup** (4/4 resolved): Rows 3-5 confirmed NO asm emission; arrayBacking_ confirmed; Row 7 error-code difference documented; **getValue() return type corrected** from `EvalResultValue` to `Value` (disasm confirmed it extracts just the Value from the last element; ~40 call sites updated). **21h.4-followup** (4/4 resolved): combine(VarSubType) done in 21h.9; combineWaveforms/constWaveform done in 21h.8; **Row 3 "vtable dispatch"** identified as inlined `Immediate` destructor (std::variant visitation, not vtable call). **~75 TODO markers remain** (down from 93 at 21f). Build clean. |
| 21b-f4 | writeToNode per-case post-tails (path A) | All 6 per-case post-tail sequences implemented. **Key structural correction**: shared `addi(addr)` common tail at 0x165c90 was typeIdx-0-only (not all cases). Per-typeIdx: 0=addi(addr)+suser(0x16); 1=8-step sine pair (addi(addr)+addi(3)+suser(0x10)+suser(0x11)+toFrequency→addi+suser(0x11)+toPhase→addi+suser(0x16)); 2=no post-tail; 3=addi(high32)+suser(0x19); 4=addi(freqHigh32)+suser(0x19)+addi(addr)+suser(**0x18** new opcode); 5=addi(addr)+suser(0x16). Protocol notes rewritten. **~75 TODO markers.** Both builds clean. |
| 21i | EParamDirection → EDirection rename | Unified the two separate `EParamDirection` (AST) and `EDirection` (Resources) enums into a single `enum class EDirection` in `types.hpp`, matching the binary's `zhinst::EDirection` (`NS_10EDirectionE`). Removed duplicate definitions from `seqc_ast_node.hpp` and `resources.hpp`. Updated ~75 call sites across 4 .cpp files: `EParamDirection::eXX` → `EDirection::eXX`, `EDirection_Read/Write` → `EDirection::eIN/eOUT`, `static_cast<EDirection>(1)` → `EDirection::eOUT`. Both builds clean (g++: 0 errors/0 warnings; clang++: 0 errors, 6 pre-existing warnings). |
| 21b-followup-3 | playIndexed/play/aux/DIO field-naming corrections | **15 TODO/placeholder markers resolved across `custom_functions.cpp`.** Three groups of fixes: **(A) playArgs.waveAssignments_ access** — `[rbp-0x440]` IS `playArgs.waveAssignments_` (at PlayArgs+0x60; offset 0x4a0−0x60=0x440). Uncommented Phase 6 validation loop + replaced `baseLen=0` sentinel with real `wavetableFront_->getWaveformSampleLength()`. **(B) devConst_ misidentification** — `[this+0x08]` loads `devConst_` NOT `config_`. Fixed 6 call sites: playIndexed Phase 15, play() Step 7, playAuxWave Phase 9, playDIOWave Phase 11 all use `devConst_->waveformGranularity` (+0x40); assignWaveIndex uses `devConst_->field_4C` (+0x4C). channelsPerGroup index corrected to `[subFunc==Aux ? 0 : 1]`. field_d0_ identified as `signal.length()` (Waveform+0x80+Signal+0x50=+0xD0). **(C) asmPlay 12-arg SysV mapping** — Full arg decode: isHold=(subFunc==Aux), fourChannel=(subFunc==DigTrigger), isBool=false, suppress=triggerMask, isHoldMode=(subFunc==Aux), trigger=0. addiEntries+placeholderEntry now pushed to results->assemblers_. **Bonus**: config_->field_18 identified as `config_->isHirzel` (2 sites in playAuxWave/playDIOWave emit-guard). Both builds clean. |
| 21j | Marker sweep: regex extraction + error codes + misc fixes | **~31 markers resolved across `custom_functions.cpp` (39→11).**
| 22d | SeqCAstNode evaluate() body expansion — complex batch | **SeqCCondExpr::evaluate** @0x223d90 (11007B): ternary conditional expression. Three paths: (1) Var: newLabel("else"/"end"), merge cond assemblers, asmBranchNode, jumpIfZero→elseLabel, evaluate both branches in sub-scopes("if"/"else"), getRegisterNumber→setValue(Var,regNum), then for each branch result: Var→addi(resultReg, branchReg, Imm(0)), Const/Cvar→value.toInt()→addi(resultReg, R0, Imm(val)), else→error 0xe4; br(endLabel), asmLabel(elseLabel), [else asm+addi], asmLabel(endLabel). (2) Const/Cvar: extract value→toInt; if nonzero: eval else first (dead), then if (live); if zero: eval if first (dead), then else (live); use live result's getValue()→setValue(0,0,value). (3) Other: error 0x1f. **SeqCFunctionCall::evaluate** @0x20c6a0 (15220B, ~350 lines): two major paths — custom functions (CustomFunctions::call with arg evaluation, signature building, name truncation) and user-defined functions (createSubScope, getFunction lookup, 5-way param binding switch on VarType with try/catch per param, body evaluate with inLoop_ save/restore, return value extraction). Full disasm audit completed: Var/Cvar cases separated, return-value guard matched to binary (extract if Void||hasError), error 0x4D arg order corrected, CFVE exception handler fully implemented (paramName→argExpr re-contextualization via std::find + setVarName). **SeqCArray::evaluate** @0x211140 (2412B): NOT ICF'd — array indexing with Wave+Const/Cvar validation, waveform lookup, bounds check, returns wave-with-index + Cvar sample. **evalCaseBody** @0x216fc0 + **evalCases** @0x216980 + **hasCases/isSingleCase/singleCase/cases** helpers all fully implemented from disasm. Prior: SeqCFunction (5080B), SeqCSwitchCase (11506B), SeqCRepeat (8567B), SeqCForLoop (9794B), SeqCDoWhile, SeqCWhileLoop, SeqCIfElse, SeqCIfCondition, SeqCCaseEntry, SeqCReturnStatement + batches 1-4. **Score: 54/54 evaluate overrides (100%) + all helpers done. Phase 22d COMPLETE.** Both builds clean. |
| 22e | Magic numbers refactoring (Category A+B) | **Category A (16 items):** Defined new enums/constants in headers: ImmediateKind (A1, value.hpp), AsmExprType (A2, asm_expression.hpp), VariantSlot (A3, value.hpp), VarFlag_Written/Frozen (A4, resources.hpp), OptFlag (A5, asm_optimize.hpp), DeviceOpts namespace (A7, device_factories.hpp), OpcodeFormat (A8, assembler.hpp), NodeTypeIdx (A9, node_map_data.hpp), CmdType (A10, assembler.hpp), RegOrder (A11, assembler.hpp), RegAction (A12, asm_optimize.hpp), kCycle constants (A13, assembler.hpp), SuserAddr (A14, device_constants.hpp), sentinel constants (A15, types.hpp), kChannelTag_I/Q (A16, types.hpp). A6 (MessageType) already existed. **Category B:** ~430 call-site replacements across 15 files: ErrorMessageT hex→names (302 sites), VarType bare ints→enum names (99 sites), AwgDeviceType bare ints→enum names (26 lines), Assembler::INVALID casts (12 sites), ValueType casts→enum names (13 sites), NodeType bare ints (2 sites). **Investigations:** SubFunc off-by-one resolved (enum 1-based is correct; play() switch reconstruction fixed from 0-based to 1-based cases), ErrorMessageT 0x2F added as UnknownError47, OpcodeFormat vs getOpcodeType distinction documented. **Deferred cleanup:** D1/D2/E1 resolved, BSS globals confirmed, WavetableManager IR kept as mirror, writeToNode typeIdx 1↔4 swap fixed in notes. Both builds clean. |
| 25 | Boilerplate reduction | **25d** promoted 5 AST-evaluate lambdas (`isConstOrCvar`, `getBackReg`, `rhsCount`, `rhsTypeOrUnset`, `rhsSubOrDefault`) from per-function duplicates to file-scope anonymous-namespace helpers in `seqc_ast_nodes_evaluate.cpp` — removed 24 duplicate definitions. **25g** added shared `inline bool isConstOrCvar(VarType)` to `resources.hpp`, replaced 5 bitwise patterns in `custom_functions_io.cpp`. **25f** confirmed LogOr `" && "` is binary-accurate (original copy-paste bug, DWORD 0x20262620). **25a-c,e** deferred: `emitLoadImmediate`/`emitRegOrConst`/`checkExternalTriggeringMode` patterns have too much per-site variation for safe mechanical replacement in binary-faithful code. |
| 24 | Python binding layer (pybind11) | **AWGCompiler** pimpl facade (8B → AWGCompilerImpl 0x2C0B, 11 forwarding methods). **AWGCompilerImpl** ctor/dtor + all 11 public methods reconstructed: compileString (validate→compile→assemble→optimize→output), compileFile (read+delegate), addWaveforms (iterate→newWaveformFromFile), writeToStream/writeToFile/writeAssemblerToFile (ElfWriter output), getCompileReport (iterate compileMessages_ + assembler report), setBinVersion/setCancelCallback/setProgressCallback (forward to compiler_), getJsonWaveformMemoryInfo (structural TODO). **compileSeqc()** orchestrator (215 lines): JSON config parsing → AwgSequencerType dispatch → DeviceType construction → AWGCompilerConfig assembly → AWGCompiler invocation → result packing. **pybind11 entry points** (233 lines): pyCompileSeqc (GIL release, arg extraction), makeSeqcCompiler/makeSeqcCompilerInCore (module.def), PYBIND11_MODULE(_seqc_compiler, m) with version attrs. **ZiFolder** utility: DirectoryType enum (Data=0, Settings=1, Executable=2), folderPath (subdir/"$Zurich Instruments"/"LabOne"/basePath_/extra), ziFolder, sessionSaveDirectoryName. CMakeLists.txt updated: find_package(pybind11) + find_package(Python3). Both builds clean. |


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

20 commits (`d15ad32`..`9b2e690`). All 259 tests green throughout.
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

14 commits (`dfc278e`..`2b23826`). All 259 tests green throughout.
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

7 commits (`3f0b24e`..`04e3ac5`). All 259 tests green throughout.
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
