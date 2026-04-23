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

**55 headers**, **82 source files**, **0 build errors**, **1 documented
placement-new warning** (libc++ vs libstdc++ string-size mismatch in
`value.cpp:237`, see `notes/libcpp_abi.md`).

Phases 1-19 complete; Phase 20 in progress (current sub-phase: **20e-ii**
Resources 19c carry-forward sweep). All class layouts verified from
disassembly. Undefined-zhinst-symbol gap = 0 (static archive
self-contained for the zhinst namespace).


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
| 19d | Undefined-symbol sweep + triage | **Planning sub-phase only.** Built `nm`-based set difference (U − T) over `libzhinst_seqc.a`: **95 truly-undefined zhinst symbols** out of 303 raw `U` entries (other 208 are inter-TU references resolved within the archive). Authoritative gap list at `/tmp/truly_undefined.txt`; full audit at `notes/undefined_symbols_audit.md` (420 lines) groups by owning class with addresses, sizes, dependencies, and current callers. **Headline finding**: `ErrorMessages::format<Args...>` accounts for 64/95 symbols (73%); a single header-inline edit clears all of them. Resulting plan: **Phase 20** (5 work-packages 20a–20e), absorbs the Phase 19c carry-forward Resources sweep into 20e. Phase 19c/19d carry-forward investigation items (Variable record-tag mismatch, readConst absW≥3 string path, GlobalResources TLS audit) tracked as 19c-followup prerequisites. |
| 19c-followup | VarType + Variable layout corrections | **All 3 followup investigations resolved.** **(1) VarType enum was wrong**: `str(VarType) @0x247dd0` jump table at 0x95c2a0 yields the correct mapping `Unset=0, Void=1, Var=2, String=3, Const=4, Wave=5, Cvar=6` (was `Unset=0, Var=2, Const=3, Cvar=4, String=5, Wave=6`). Tag mapping confirmed across all add/read overloads — there is **no** separate "record-tag" enum; Variable+0x00 IS the VarType. **(2) Variable layout was wrong**: disasm of readConst @0x1e7d70 (with r12=Variable*) shows `which_` at +0x10 (not +0x08) and variant storage at +0x18 (not +0x10). New layout adds a `subType` (VarSubType) field at +0x08. Size 0x58 confirmed. **(3) TLS layout fully mapped**: 5 zhinst slots in shared `.so` TLS module block — `AsmList::Asm::createUniqueID::nextID` (+0x40), `Node::idCounter_` (+0x44), `GlobalResources::regNumber` (+0x48), `labelIndex` (+0x4c), `random[313]` MT19937-64 state (+0x50, 2504B). Total zhinst TLS = 0x9D8. **Cascading fix applied**: VarType enum + Variable struct rewritten in `resources.hpp`; `str(VarType)`, `toString`, `addConst`, `readConst` (string path filled with new field names) updated in `resources.cpp`; `Variable::~Variable` body filled in; `expression.cpp:177` corrected from `VarType_Const` to `VarType_String` (numeric value 3 unchanged); doc-comments in `eval_results.cpp` and `custom_functions.cpp` re-labeled. **`Node::idCounter_` declaration moved into `Node` class** (was free static `node_id_counter`). `notes/struct_layouts.md` extended with VarType + VarSubType + Variable layout reference tables. Build clean. |
| 20a | Globals + ErrorMessages template body | **WP-A complete: 69/95 truly-undefined symbols resolved (73% gap closure) in one sub-phase.** **(1) ErrorMessages::format<Args...>** body inlined into `error_messages.hpp:457-466` using `boost::format` chained `operator%` driven by `std::initializer_list<int>{...}` (C++17 — safer than fold-expr w.r.t. operator-precedence pitfalls). All 64 distinct template instantiations now generated as weak symbols `W` at the 14 caller TUs. **(2) `errMsg` global** defined in `error_messages.cpp:28` (BSS @ 0x95de60). **(3) `Prefetch::minIndexedSize` static int** defined in `prefetch.cpp:2294` with init value `0x1000` (4096), recovered from `__cxx_global_var_init` at 0xd4361 (`mov DWORD PTR [rip+...],0x1000  # 0xb846d8`). **(4) Three `extern const std::string` globals** (`zsyncDataPqscDecoder`, `zsyncDataPqscRegister`, `constAwgIntegrationTrigger`) defined in `error_messages.cpp:45-47`. String contents recovered from binary rodata via `strings _seqc_compiler.so`: `"ZSYNC_DATA_PQSC_DECODER"`, `"ZSYNC_DATA_PQSC_REGISTER"`, `"AWG_INTEGRATION_TRIGGER"`. Note: binary mangles with `L` prefix (internal linkage); our header declares `extern` for cross-TU access — slight ABI deviation, behaviour unchanged. Build clean. Remaining gap: 26/95 symbols across WP-B through WP-E. |
| 20b | Trivial Rule-of-Five and missing default ctors | **WP-B complete: 8 symbols resolved in one sub-phase. Total now 77/95 (81%).** **(1) `Immediate` copy/move/copy-assign** added to `value.cpp` with per-index switch dispatch matching existing `~Immediate`/`operator int` pattern (placement-new + per-case copy/move; no vtable dispatch — semantically equivalent). **(2) `Value()` default ctor**: `type_=Unspecified, which_=0, storage_.i=0`. Any toX() will throw — matches "uninitialized variable" path. **(3) `Node()` default ctor**: delegates to 3-arg ctor with `(NodeType{0}, 0, -1)`. **(4) `WaveformFile(const char*)` ctor**: copy-constructs `name`, zero-inits POD fields. Important finding: binary's 0x2a7ff0 was a `std::construct_at<>` specialization that inlined the body — no dedicated ctor symbol existed in the binary. **(5) `floatEqual(double,double)` @0x2ec050**: **Surprise** — despite the name, binary body is `cmpeqsd xmm0, xmm1` (exact IEEE-754 bitwise equality), NOT a tolerance check. Callers use it to test for exact zero (e.g. `floatEqual(beta, 0.0)`) which `==` handles identically. **(6) `logExceptionToClog` namespace fix**: `log_exception.cpp` corrected from `zhinst::detail` to `zhinst::logging::detail` to match binary mangling and `logging.cpp` declaration site. All 8 symbols verified `T` in archive. Build clean (1 pre-existing libc++/libstdc++ string-ABI warning, documented). |
| 20c | Wavetable/Waveform ctors and template instantiations | **WP-C complete: 5 symbols resolved (audit listed 6, but `WaveformFile(const char*)` was already cleared in 20b). Total now 82/95 (86%).** **(1) `WaveformIR(string, Type, DC&)` ctor** body recovered from disasm at `0x2aa170-0x2aa20f` (the `allocate_shared<WaveformIR>` dispatcher inlined the ctor — no standalone symbol). Body: copies name+type, zeroes Waveform middle area, `waveIndex=-1`, `seqRegWidth = dc.waveformGranularity (dc+0x40)`, `deviceConstants=&dc`, IR-extension fields all zero, `irField2 = dc.field_24 (dc+0x24)`. **(2) `WaveformFront(string, Type, DC&)` ctor** body recovered from disasm at `0x29b110-0x29b24f` (`newWaveformFromFile` dispatcher). Body identical to IR variant in the Waveform-base region; Front-extension area sets `frontField1 = 1` (notable: IR sets the equivalent byte to 0). **(3) Misleading header comment in `waveform_front.hpp:65` fixed** — the original `"bitsPerSample=dc[0x40]"` was wrong on two counts: dc[0x40] is `waveformGranularity` (not bitsPerSample, which is at +0x50), and the destination field is `seqRegWidth` (+0x70), not bitsPerSample. Replaced with verified offset documentation. **(4) `WaveIndexTracker<WaveformFront/IR>` template ctors** — added explicit-instantiation lines at end of `wave_index_tracker.cpp`; symbols emit as `W` (weak), satisfying the U references at link time. Also fixed pre-existing typo: template body referenced `wp->playIndex` but the field is named `waveIndex` at Waveform+0x6C. **(5) `WavetableManager<WaveformIR>::insertWaveform` specialization** added to `wavetable_manager_ir.cpp` mirroring the WaveformFront body; required a forward-declared specialization at top-of-file because the same TU's earlier ctor and `newWaveform` already implicitly use the function (C++14 [temp.expl.spec]/6). **Note on `nm -u` count**: raw count remains 377 (per-TU `U` references survive in static archives even when `T` exists elsewhere); per-symbol verification with `nm | grep " T \$sym\$"` (or `W`) confirms all 5 targets defined. Build clean. |
| 20d | AWGAssembler/parser/NodeMap small methods | **WP-D complete: 7 symbols resolved in one sub-phase. Total now 89/95 (94%).** **(1) `AWGAssembler::assembleStringToExpressionsVec`** wrapper uncommented in `awg_assembler.cpp:40-47` with correct return type `std::vector<std::shared_ptr<AsmExpression>>`. **(2) `AWGAssemblerImpl::extractComment`** body added at end of `awg_assembler_impl_pipeline.cpp` (find `"//"` substring → return suffix). The binary inlined this twice as the `$_1` lambda inside `assembleStringToExpressionsVec`; we externalize it as a real method to satisfy the cross-TU U reference. **(3) `SeqcParserContext::raiseError` + `setSyntaxError`** implemented in new `src/seqc_parser_context.cpp` using raw byte-offset access (the class is opaque in the header — never instantiated in reconstructed code, only used by-pointer). raiseError uses an indirect callback at `[this+0x30]` (vtable[6]/+0x30 path) with `std::clog` formatted-output fallback; setSyntaxError is a one-byte write at `[this+0x3]`. **ABI risk noted**: the vtable offset is libc++-specific. **(4) `NodeMap::toFrequency(double, double)`** = `(int64_t)(freq * 2^48 / sampleClock)` reinterpreted as uint64_t. Constant `2^48 = 281474976710656.0` from rodata `0x956078 = 0x42f0000000000000`. **(5) `NodeMap::toPhase(float)`** = `roundf(value * (2^23/360))` then 23-bit two's-complement wrap. Scale constant `0x46b60b61 ≈ 23301.689453125f` from rodata `0x8fd2b4` (= 2^23/360). Wrap logic shared with `pauPoffIwrap` (extracted as `wrap23()` helper in anonymous namespace). **(6) `parseOptionalRate`** implemented at end of `custom_functions.cpp`. Disasm-derived semantics: header parameter naming is misleading; the third iterator (rdx) is the *parse cursor* (typically the return of `PlayArgs::parse()`). If exactly one EvalResultValue remains between cursor and `end`, calls `getPlayRate()`. Otherwise encodes the "no-rate" sentinel as `((strict ? 1 : 0) - 1) | 5` (5 if strict, 0xffffffff if not). Throws `CustomFunctionsValueException(format(0xee, name), itemIndex)` if extra unparsed args remain. **Side finding**: libstdc++ vs libc++ ABI mangling — when verifying with `nm | grep " T $sym\$"` using libc++-style mangled names (`__1`, `__wrap_iter`), they DO NOT match, but the static archive is internally consistent — both definitions and per-`.o` U references use the libstdc++ mangling, so they resolve at link time. The clean `cmake --build` (zero warnings, zero unresolved-at-link-time) is the ground truth. Build clean. |
| 20e-i | util::wave + MemoryAllocator + CsvParser cheap wins | **WP-E-i complete: 6 symbols resolved in one sub-phase. Final undefined-zhinst-symbol gap = 0 (all 95 originally-cataloged + the 6 WP-E-i targets now defined; static archive fully self-contained for the zhinst namespace).** **(1) `util::wave::double2awg`** @0x299630 — 14-bit signed sample with 2 marker bits in low. Constants from rodata: 1.0 (`0x956030`), -1.0 (`0x956068`), 8190.0 (`0x956330`). Saturate path: if sample > 1.0, use 8190 directly; else `max(-1.0, sample) * 8190`, then `lround`. Pack: `(rounded << 2) \| (marker & 0x3)`. **(2) `util::wave::double2awg1m`** @0x299680 — 15-bit signed sample with 1 marker bit. Scale 16383.0 (`0x956338`). **(3) `util::wave::double2awg16`** @0x299700 — 16-bit signed sample, no marker. Scale 32767.0 (`0x956340`). **(4) `util::wave::hash`** @0x299760 — **uses `boost::uuids::detail::sha1`** internally (binary calls `process_block` @0x29a270 and `get_digest` @0x29a000). Returns `vector<uint32>` of 5 words (160-bit SHA-1, MSB-first per word). Reconstructed via `boost::uuids::detail::sha1::process_bytes` + `get_digest(uchar[20])` with manual MSB-first packing into uint32 to match binary's bswap-then-store sequence. **Resolves open question #1**: the IV constants at `0x8fc720` (`67452301 efcdab89 98badcfe 10325476 c3d2e1f0`) are the standard SHA-1 initial state; archived Phase 12c 'no CsvParser symbols' note was wrong on a related front (see #6 below). **(5) `MemoryAllocator(const DeviceConstants*, uint32_t)` ctor** — inlined at all binary call sites (no standalone symbol), but the build host emits an unresolved reference from any TU that takes the ctor's address. Provided real ctor matching documented 0x70-byte layout: copies `dc` and `startOffset`, sets `lastAllocEnd_=0xFFFFFFFF`, derives `memorySizeInSamples_/cacheLineSize_/maxBlocksPerCL_` from `dc->{waveformMemorySize,waveformAlignment,cachePageCount}`, initializes per-CL ownership table to all `0xFFFFFFFF` (free), `numCacheLines_ = memorySize / cacheLineSize` (with cacheLineSize=0 guard for non-cached families). **(6) `CsvParser::csvFileToWaveform<WaveformIR>`** @0x2be830 — STUB. Full reconstruction deferred (~7000-byte function with `0x528` stack frame, complex CachedParser/CSV-parsing pipeline, not on hot path for non-CSV SeqC programs). Stub throws `std::runtime_error` so callers fail loudly rather than silently producing zero-filled waveforms. **Resolves open question #1** definitively: binary contains `csvFileToWaveform<WaveformFront>` @0x2ba8b0 AND `<WaveformIR>` @0x2be830 as full template specializations (not inlined as Phase 12c claimed). New TUs: `src/util_wave.cpp`, `src/csv_parser.cpp`. Build clean, no warnings. |

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
│   ├── frontend_lowering.hpp    # FrontendLoweringContext (0x50), FrontendLoweringState (0x30)
│   ├── eval_results.hpp         # EvalResults (0x80), 14 methods
│   ├── eval_result_value.hpp    # EvalResultValue (0x38B struct)
│   ├── exception.hpp            # zhinst::Exception (0x60), ZIAWGCompilerException
│   ├── custom_functions.hpp     # CustomFunctions (0x1E0) + AccessMode + PlayArgs + exceptions
│   ├── math_compiler.hpp        # MathCompiler (23 single-arg + 5 multi-arg fns) + MathCompilerException
│   ├── node_map_data.hpp        # NodeMapData abstract base + Virt/DirectAddr subclasses + NodeMapItem (0x18)
│   ├── cached_parser.hpp        # CachedParser (0x60), CacheEntry, CachedFile
│   ├── waveform_generator.hpp   # WaveformGenerator (0xC0) + exception classes
│   ├── logging.hpp              # Severity (8 levels), ZiLogger tag, detail::LogRecord RAII
│   └── tracing.hpp              # TraceProvider singleton, ScopedSpan RAII, getDefaultLabOneResource
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
│   ├── custom_functions.cpp          # CustomFunctions class (~5250 lines); PlayArgs fully reconstructed,
│   │                                 # ALL 37 builtin one-liner stubs replaced with real implementations.
│   │                                 # Remaining partial stubs: mergeWaveforms (body throws), setRate (partial),
│   │                                 # setInt/setDouble (delegate to unimplemented writeToNode 23KB).
│   │                                 # Key discoveries: readConst returns EvalResultValue (not void),
│   │                                 # PRNG registers 0x74-0x77, waitState_ protocol for DIO/ZSync.
│   ├── math_compiler.cpp             # MathCompiler: 23+5 wrappers + dispatch, fully complete (197 lines)
│   ├── node_map_data.cpp             # NodeMap hierarchy — fully reconstructed (Phase 17a)
│   ├── eval_result_value.cpp         # EvalResultValue dtor (extracted Phase 16b)
│   ├── cached_parser.cpp             # CacheEntry/File + 8 method stubs
│   ├── waveform_generator.cpp        # 2305 lines, all 32 DSP functions reconstructed, 2 exception classes
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

Highest-value open items:
- **#10**: smap remaining logic — ~0x1E6 bytes after alui call
- **simplifyAssign** @0x280e10 and **splitReg** @0x281000 still have
  pre-correction register field usage (see `optimization_passes.md`)
- **reg0/reg1/reg2 naming** in assembler.hpp remains misleading —
  renaming deferred (cascading rename across 20+ files)

These are also tracked in the Deferred section of `TODO.md`; the
duplicate listings here are kept as a quick-reference and may be
collapsed during a future overview-cleanup pass.
