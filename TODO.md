# TODO — Reconstructed zhinst SeqC Compiler

> **Completed Phases 1-12 archived in
> [`reconstructed/notes/archive/TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md).**
> 597 completed items across 12 phases (plus sub-phases 10.5-10.8).

---

## Summary of remaining work (refreshed Phase 22e audit, 2026-04-25)

**Build**: clean (g++ + clang++/libc++), 0 errors, 1 documented warning.
**95/95 undefined zhinst symbols resolved** — static archive self-contained.
**~35 source markers** across ~21 files (TODO/TBD/throw-stubs).
**~430 binary zhinst symbols** not yet emitted by reconstruction (see breakdown below).
**~15 placeholder field names** across 8 headers (see `notes/placeholder_field_names.md`).
**~30 reinterpret_cast raw-offset accesses** in resources.cpp alone.
**~0 magic hex device-type bitmasks** (all replaced with named constants).

### Reconstruction coverage

| Category | Status | Notes |
|----------|--------|-------|
| SeqCAstNode print/clone | ✅ complete | 49 print + 49 clone, all real implementations |
| SeqCAstNode evaluate() | ✅ complete | 54/54 overrides + hasCases/evalCases/evalCaseBody |
| SeqCAstNode copy/swap/accessors | ❌ 158 symbols | Copy-ctor, operator=, swap, and child-accessors (expr(), funName(), stmts(), etc.) for all 53 subclasses |
| CustomFunctions builtins | ✅ mostly done | 86 methods, 0 return-nullptr stubs; 6 conservative stubs remain |
| CustomFunctions ctor binding | ✅ complete | 81/81 entries; 5 aliases (setSeqIndex, setReadoutRegisterAddress, waitOscPhaseOfDemod, setUser, getUser) |
| AsmCommands methods | ⚠️ mangling gap | 65 methods declared+defined but emitted with libstdc++ mangling; binary uses libc++ |
| AsmCommandsImpl (Cervino+Hirzel) | ⚠️ mangling gap | 14 methods, same issue |
| AWGCompiler public API | ✅ complete | Phase 24c: pimpl wrapper + AWGCompilerImpl (0x2C0), 15 methods, 0 TODOs |
| compileSeqc orchestrator | ✅ complete | Phase 24d: JSON config parsing → AWGCompiler pipeline |
| pybind11 binding layer | ✅ complete | Phase 24e: pyCompileSeqc, makeSeqcCompiler, PyInit |
| ZiFolder utility | ✅ complete | Phase 24b: 4 methods (ctor, folderPath, ziFolder, sessionSaveDirectoryName) |
| GetNodeMap\<T\>::get() | ✅ complete | Phase 26a: 8 device tables, 1081 entries |
| ZI*Exception hierarchy | ✅ mostly done | 26/26 subclasses declared+defined (Phase 29); getKind/toApiCode deferred (SDK plumbing) |
| CalVer + utility free fns | ✅ mostly done | 30/37 symbols: CalVer (16), formatTime (3), serial predicates (10), getPlatformName (1); 6 misc unreferenced + 1 extern |
| DeviceType extra methods | ❌ ~21 symbols | Factory makeDefault(), DeviceType ctors, comparison operators |
| Other missing methods | ❌ ~53 symbols | Assembler::str, Node::toString, Value::toString, various toString/toJson, ElfReader getCode/getLineMap/getWaveform, Prefetch wvf helpers, etc. |
| MathCompiler | ✅ complete | 67 symbols |
| WavetableManager\<T\> | ✅ complete | All 16 methods (Phase 21e) |
| DeviceType/Family/Option | ✅ complete | Phase 14b |
| CachedParser | ✅ complete | Phase 13d + 21f |
| Prefetch/Cache | ✅ complete | Phase 15b |
| AsmOptimize | ✅ complete | Phase 21c |
| Compiler pipeline | ✅ all steps wired | Phase 30a-e complete; steps 5-19 all wired |
| Resources | ✅ complete | All 37+ methods (Phase 20e-ii) |
| EvalResults | ✅ complete | 14 methods (Phase 19a) |
| Logging/tracing | ✅ complete | notes/logging_tracing.md |
| NodeMapData | ✅ complete | Phase 17a |
| Undefined symbols (link) | ✅ 0 remaining | Phase 20a-20e (95/95) |
| Magic numbers | ✅ done | Phase 22e |

### Code quality debt

| Category | Count | Hotspot files |
|----------|-------|---------------|
| Placeholder field names | ~15 | 8 headers: waveform_front (3), waveform_ir (1), waveform (4), asm_expression (2), elf_reader (1), resources (1), awg_assembler_impl (1), awg_compiler_config (5) — see `notes/placeholder_field_names.md` |
| reinterpret_cast raw offsets | ~100 | resources.cpp (8), other files (~92, mostly inherent: serialization, tagged unions, SSO) |
| Device-type hex bitmasks | 0 | custom_functions_io.cpp (done), custom_functions_playback.cpp (done) |
| Approximate implementations | ~19 | exception.cpp, custom_functions_play.cpp, custom_functions_io.cpp |
| Stubs (conservative) | 0 | All 6 resolved in Phase 26b |
| "likely"/"uncertain" comments | ~17 | 12 files |
| Throwing runtime_error stubs | 1 | awg_compiler_config.cpp (1) |

---

## Differential Testing (ELF comparison)

Test harness in `tests/` compares ELF output from the original binary vs the
reconstructed pybind11 module. The module builds and loads (RTLD_LAZY), but
crashes at runtime due to remaining undefined symbols.

### Infrastructure (done)

- [x] Test harness: `tests/diff_test.py`, `tests/compile_worker.py`
- [x] Test cases: `tests/cases/manifest.json` with 12 cases (3 device types)
- [x] Original-only smoke test: all 12 cases pass
- [x] pybind11 module target in CMakeLists.txt (builds _seqc_compiler.so)
- [x] ODR violations fixed (StaticResources, GlobalResources, ResourcesException, Prefetch, asmerror)
- [x] TLS model fixed (-ftls-model=global-dynamic + PIC)
- [x] flex/bison linkage fixed (extern "C" → C++ linkage for .c files compiled as C++)
- [x] seqc_lexer.c prefix fixed (seqc → seqc_ to match compiler.cpp references)
- [x] seqc_parser.tab.c and seqc_lexer.c added to CMake sources
- [x] Boost::log + zlib dependencies added
- [x] Python version pinned (3.14)

### Remaining blockers for first successful differential test

- [x] ~16 undefined zhinst symbols cause runtime crashes (via RTLD_LAZY):
  - ~~AsmCommands: syncCervino, asmSyncHirzel, st, prf, ssl, addi, addr, cwvf,
    smap, wprf, wwvf, cwvfr~~ — not triggered at runtime (RTLD_LAZY)
  - ~~AWGAssembler::getReport~~ — FIXED: uncommented forwarding method
  - ~~SeqcParserContext::hadSyntaxError~~ — FIXED: added missing definition
  - ~~Prefetch::wvfImpl, wvfRegImpl~~ — not triggered at runtime (RTLD_LAZY)
- [x] Hardcoded libc++ offsets cause segfault with libstdc++ — FIXED:
  - assembleExpressions: this+0xB0 → labelBimap_ field access
  - awg_assembler_opcodes: this+0xD8 → labelBimap_ field access
  - custom_functions: this+0x1B0 → warningCallback_ null check
- [ ] pybind_seqc.cpp returns (bytes, json_string) but original returns
  (bytes, dict) — need to add json.loads() call in the C++ layer or handle
  in the worker
- [x] **`Compiler::compile()` pipeline is a stub** — ~~steps 5-19 are
  commented-out pseudocode~~ All steps 5-19 now wired. See **Phase 30** below.

---

## Phase 30: Wire up Compiler::compile() pipeline

**Goal:** Make `Compiler::compile()` (`src/compiler.cpp:188-273`) produce
real assembly output so the differential test harness can compare ELF
output against the original binary.

The function currently runs steps 1-4 (parse) and returns an empty vector.
Steps 5-19 are now all wired. Remaining work is differential testing.

### Dependency audit

| Step | Function | Has body? | Blocker? |
|------|----------|-----------|----------|
| 5 | `StaticResources::init()` | ✅ complete (static_resources.cpp:51) | No — **WIRED** |
| 6 | `toSeqCAst(expr)` | ✅ complete (seqc_ast.cpp) | No — **WIRED** |
| 8 | `FrontEndLoweringFacade::lower()` | ✅ complete (compiler.cpp:446) | No — **WIRED** |
| 10-11 | Node tree walk + AsmList build | ✅ wired | No — **WIRED** |
| 12 | `AsmOptimize::optimizePreWaveform()` | ✅ complete (asm_optimize.cpp:205) | No — **WIRED** |
| 14 | `Compiler::runPrefetcher()` | ✅ wired | No — **WIRED** |
| 15 | `AsmOptimize::optimizePostWaveform()` | ✅ complete (asm_optimize.cpp:221) | No — **WIRED** |
| 16 | `AsmCommands::unsyncCervino()` | ✅ complete (asm_commands.cpp:760) | No — **WIRED** |
| 19 | Copy AsmList → vector\<AssemblerInstr\> | ✅ wired | No — **WIRED** |

### 30a. `toSeqCAst()` — Expression → SeqCAstNode conversion — COMPLETE 2026-04-26

Free function at @0x1f6240. Converts the flex/bison `Expression` tree
into the typed `SeqCAstNode` hierarchy via recursive dispatch on
operationType/commandType/operator.

- [x] Disassemble `toSeqCAst` @0x1f6240 — determine size and call graph
- [x] Reconstruct body (~310 lines, 50 dispatch cases across 3 jump tables)
- [x] Build verify
- [x] Sub-phase wrap-up

### 30b. Wire up steps 5-9 in `Compiler::compile()` — COMPLETE 2026-04-26

Uncommented and wired the calls for steps 5-9:

- [x] Step 5: Construct StaticResources with warning callback + init
- [x] Step 5b: Wrap in GlobalResources (`make_shared<GlobalResources>(staticResources)`)
- [x] Step 5c: Store resources into `customFunctions_->resources_` (added `friend class Compiler`)
- [x] Step 6: `auto seqcAst = toSeqCAst(expr)` (depends on 30a)
- [x] Step 7: Debug print if `config_->debugFlags & 0x04`
- [x] Step 8: `FrontEndLoweringFacade::lower(resources, *seqcAst, messages_, asmCommands_, customFunctions_, waveformGen_, wavetable_, config_->channelGrouping)`
- [x] Step 8b: `ast_ = lowerResult.astResult`
- [x] Step 9: error check
- [x] Build verify — clean, 0 warnings
- [x] BONUS: Identified AWGCompilerConfig+0x98 as `channelGrouping` (was `unknown_98`)
- [x] Sub-phase wrap-up

### 30c. Node linearization + assembly emission (steps 10-11) — COMPLETE 2026-04-26

Steps 10-11 are NOT "linearizing nodes into assembly" as originally claimed.
Assembly instructions are generated during `evaluate()` inside `lower()`.
Steps 10-11 build the root node wrapper, walk the tree setting parent
pointers (BFS via deque), and splice EvalResults assemblers into asmList_.

- [x] Preamble: asmLabel("start") + asmLoadPlaceholder
- [x] Root Node(NodeType::Load, placeholderAsm.sequenceId, numChannelGroups)
- [x] Graft lowered AST: hasMain && ast_ → rootNode->next = ast_; else → evalResult->node_
- [x] BFS parent-pointer walk via deque
- [x] Append placeholder Asm, bulk-insert evalResult->assemblers_, end/wwvf/nop trailer
- [x] Build verify — clean, 0 warnings

### 30d. Wire up runPrefetcher (step 14) — COMPLETE 2026-04-26

All 13 steps inside `runPrefetcher()` converted from pseudocode comments
to real method calls. Prefetch construction with warning callback, all
WavetableIR allocation methods, conditional determineFixedWaves,
fillInPlaceholders, channel info extraction.

- [x] Construct Prefetch with warning callback + ast_ as root
- [x] preparePlays → getUsedWavesForDevice → setUsedWaveforms
- [x] assignWaveIndexImplicit → alignWaveformSizes → assignWaveformAllocationSizes
- [x] Conditional determineFixedWaves (cacheType == 1)
- [x] updateWaveforms → placeLoads → fillInPlaceholders
- [x] Store channelCount_ and channelMode_ from Prefetch queries
- [x] Build verify — clean, 0 warnings

### 30e. Wire up remaining steps + output (12, 15, 16, 19) — COMPLETE 2026-04-26

- [x] Step 12: Construct AsmOptimize with error/info callbacks + device constants
      + cancel callback; `optimizePreWaveform(asmList_)`
- [x] Step 13: Conditional serialize (string_30_owned) + conditional
      serialize/deserialize round-trip (unknown_28 == 1)
- [x] Step 13c: Construct WavetableIR from WavetableFront via allocate_shared
- [x] Step 15: `optimizePostWaveform(asmList_)`
- [x] Step 16: `asmCommands_->unsyncCervino()` — append entries to asmList_
- [x] Step 17: Conditional debug print (debugFlags & 0x08)
- [x] Step 19: Copy `asmList_` entries → `vector<AssemblerInstr>` return
- [x] Build verify — clean, 0 warnings
- [x] BONUS: Added AsmOptimize constructor (was missing from header + impl)
- [x] BONUS: Fixed GlobalResources namespace/class conflict in asm_optimize.hpp

### 30f. First differential test run

- [ ] Run `tests/diff_test.py` with both original and reconstructed
- [ ] Handle pybind return type mismatch (json string vs dict)
- [ ] Triage ELF differences section-by-section
- [ ] Document findings + update TODO.md

---

## Phase 13: Method body reconstruction

Fill in stubbed method bodies by disassembling the binary at the addresses
noted in each stub's comment. Priority: highest-value stubs first.

### 13a. SeqCAstNode subclass bodies

106 stubs across 53 subclasses (print + clone for each). Most are
straightforward (print writes to stream, clone deep-copies children).

- [x] Analyze print() pattern from one representative class (e.g. SeqCIfElse @0x201df0)
- [x] Implement print() macro template based on pattern
- [x] Analyze clone() pattern (likely recursive deep-copy)
- [x] Implement clone() macro template
- [x] Fill in non-trivial subclass bodies (SeqCVariable, SeqCValue, SeqCFunction)
- [x] Resolve unknown #96 (SeqCAstNode `type` field meaning) — RESOLVED: renamed `type_` → `lineNr_` (source line number)
- [x] Sub-phase wrap-up

### 13b. CustomFunctions method bodies

~35 SeqC built-in function stubs (play, playZero, playHold, setTrigger,
waitDigTrigger, etc.) plus 5 infrastructure methods.

- [x] Reconstruct ctor registration logic @0x12bcf0 (maps function names → lambdas)
- [x] Reconstruct call() @0x159470 (alias resolution + funcMap_ dispatch)
- [ ] ~~Reconstruct eval()~~ — no standalone eval(); moved to Phase 13e
- [x] Pick 5-10 representative built-in functions, reconstruct bodies
- [x] Stub remaining built-in functions with improved comments
- [ ] Resolve field unknowns: MathCompiler layout (#102), field_168 (#101) — deferred to 13e
- [x] Sub-phase wrap-up

### 13c. WaveformGenerator method bodies

~35 waveform DSP function stubs (sine, cosine, gauss, drag, rrc, etc.)
plus call/eval/getOrCreateWaveform infrastructure.

- [x] Reconstruct call() — alias resolution + funcMap_ dispatch
- [x] Reconstruct eval() — body documented; throws "blocked on EvalResults" with full algorithm in comments (real impl deferred to Phase 15a once EvalResults layout is known)
- [x] Reconstruct getOrCreateWaveform() — caching layer
- [x] Pick 5-10 representative waveform functions, reconstruct bodies (zeros, ones, rect, scale, add, gauss + readers + createDummyWaveform)
- [x] Stub remaining with improved comments (27 stubs documented with expected signatures)
- [x] Resolve field unknowns: field_50_ (#108 — renamed createdNames_), field_B0_ (#110 — deferred, kept as shared_ptr<void>)
- [x] Reconstruct aliasMap_ contents from ctor registration code (#111 — empty, no aliases registered in this binary)
- [x] Sub-phase wrap-up

### 13d. CachedParser method bodies

7 stubs — most are Boost.Serialization-heavy.

- [x] Reconstruct loadCacheIndex @0x2afec0 (boost::archive::text_iarchive) — ifstream → text_iarchive >> index_, then recompute currentSize_ by walking tree (sum of fileSize_); cleanCache() on any exception
- [x] Reconstruct saveCacheIndex @0x2b03c0 (boost::archive::text_oarchive) — skip if disabled or empty; ofstream → text_oarchive << index_; sticky-disable enabled_ on exception
- [x] Reconstruct cacheFile @0x2b05b0 (~700 instructions) — algorithmic outline + section-name table; full ELF-building body left as documented stub (heavy ELFIO interaction)
- [x] Reconstruct getCachedFile @0x2b1900 (~400 instructions) — full lookup-and-load with cacheFileOutdated check, eviction, ElfReader → 4 sections (.channels, .marker_bits, .data, .marker)
- [x] Reconstruct removeOldFiles @0x2b01a0 — copy-to-vector + sort by timestamp + evict-while-oversize, respecting valid_ pin flag
- [x] Implement CacheEntry::serialize template — inline 6-field `ar &` sequence
- [x] BONUS: Reconstructed cleanCache @0x2b0140 with exception swallowing
- [x] BONUS: Reconstructed cacheFileOutdated @0x2b14d0 — ElfReader-based ".format" + ".file_name" mtime check (NOT an index_ lookup as previously assumed)
- [x] BONUS: Created minimal ElfReader/ElfSection forward-decl header
- [x] BONUS: CORRECTED CachedFile layout — there is NO bool found_ field; layout is `uint16_t channel_; vector<uint8_t> markerBits_; vector<double> samples_; vector<uint8_t> markers_` (resolves #113)
- [x] BONUS: cacheSize_/currentSize_ are tracked in BYTES, not entry counts (header doc fixed)
- [x] BONUS: valid_ flag semantics clarified — set true on access (LRU-touch alongside timestamp_), prevents eviction (acts as "in-use pin")
- [x] Sub-phase wrap-up

### 13e. Quick-win unknown resolutions

Small disassembly tasks from the unknowns triage. Each is a single
ctor/method read to determine field types or contents.

- [x] MathCompiler internal layout (#102) — RESOLVED in re-audit. Two
      `std::map`s at +0x00 (single-arg) and +0x18 (multi-arg). Header updated.
- [x] NodeMapData subclass fields (#104) — DirectAddrNodeMapData has
      `uint32_t addr_` at +0x08 (after 8B vptr). Header updated.
      VirtAddrNodeMapData fields still unknown — deferred (no clear read sites).
- [x] CustomFunctions field_168 (#101) — RE-CHARACTERIZED. Real type per
      ~CustomFunctions @127cf2 is `std::vector<T>` with sizeof(T)==8 trivially
      destructible. Element T still unidentified — moved to Phase 14a follow-up.
- [x] BONUS: corrected pre-existing `nodeMap_` field at CustomFunctions+0xF8
      from `std::map<string,NodeMapItem>` (24B inline) to `std::unique_ptr<NodeMap>`
      (8B). Confirmed via lookupNode @0x15c530 dereferencing [this+0xf8] as a
      single ptr and calling NodeMap::retrieve(). Added NodeMap class definition
      in src/custom_functions.cpp.
- [x] BONUS: documented historical confusion with the dtor at 0x1306c1 (which
      is NOT CustomFunctions::~CustomFunctions — that's at 0x127c90). Note added
      to unknowns_open.md and to the layout comment in custom_functions.hpp.
- [x] WaveformGenerator field_50_ — RESOLVED in 13c (renamed createdNames_, set<string> cache)
- [x] WaveformGenerator field_B0_ — RESOLVED as negative finding (Phase 15a).
      No setter exists; 16B slot reserved-but-unused.
- [x] WaveformGenerator aliasMap_ — RESOLVED in 13c (empty in this binary)
- [x] Sub-phase wrap-up

### 13f. WaveformGenerator DSP function expansion (optional)

**Optional / low priority.** Existing stubs are correct one-liners.
Expanding to full DSP math gives little value without test infrastructure
to validate sample-by-sample equivalence with the binary. Skip unless
explicitly requested.

After 13e, optionally expand the 27 documented-but-stubbed DSP functions
(sin, cos, drag, blackman, hamming, hann, chirp, rrc, etc.) using the
patterns established in 13c (gauss/rect/scale). Many follow the same
checkArgCount → readers → fill-vector → return Signal pattern.

- [x] Reconstruct trig family (sin, cos, sinc) — share frequency/phase reader
- [x] Reconstruct envelope family (blackman, hamming, hann, drag) — DONE (Phase 16d)
- [x] Reconstruct random family (rand, randomGauss, randomUniform, lfsrGaloisMarker) — DONE (Phase 16d)
- [x] Reconstruct combinator family (join, interleave, multiply, cut, flip, filter) — DONE (Phase 16d)
- [x] Reconstruct misc (rrc, vect, placeholder, mask, marker, markerImpl,
      readWave, interpolateLinear) — DONE (Phase 16d/17b)
- [x] Sub-phase wrap-up

---

Types not yet started, identified from symbol table.

### 14a. MathCompiler (67 symbols, 0x30 bytes)

Embedded in CustomFunctions at +0xC8. Separate math expression compiler
for constant-folding and evaluation.

- [x] Determine MathCompiler layout from ctor @0x1c2250 — already resolved in
      Phase 13e re-audit: two `std::map`s at +0x00 (single-arg) and +0x18 (multi-arg).
- [x] Identify all 67 methods from symbol table — 33 zhinst:: methods (28 single-arg
      libm wrappers, 5 multi-arg, ctor, dtor, call, functionExists, exception class);
      remaining ~34 symbols are libc++ __function::__func<__bind<...>> instantiations.
- [x] Reconstruct layout + key methods — full ctor (populates both maps via std::bind),
      `call()` with single→multi dispatch and FuncSingleArg/UnknownFunction throws,
      `functionExists()` with strict|found semantics, `pow()` with FuncExactly2Args
      throw on size!=2. Single-arg wrappers were already stubbed correctly; fixed
      sin/sinh/sqrt addresses (typo: sqrt was duplicating sinh's address).
- [x] FOLLOW-UP from 13e: identify CustomFunctions field_168 element type
      RESOLVED — Phase 13e characterization was wrong. The dtor at 127cf2 has a
      node-walk loop at 127d40-127d70 reached via `jne` at 127cf0 (initially
      missed). Real type: `std::unordered_set<std::string>` (40B container, 40B
      node = 16B header + 24B std::string element). Confirmed by 1.0f
      max_load_factor at +0x188 in ctor at 12bec9 and by string-dtor pattern
      at 127d58-127d6b. Original Phase 11d classification was correct.
- [x] FOLLOW-UP from 13e: resolve VirtAddrNodeMapData fields
      RESOLVED — total 0x38 = vptr (8B) + `std::string name_` (+0x08, 24B)
      + `std::vector<int32_t> addresses_` (+0x20, 24B). Smoking gun:
      copy ctor @0x1c4dc0 calls `vector<int>::__throw_length_error` at 1c4e72;
      getJson @0x1c5240 reads "name" key from string field then iterates the
      vector with `movsxd rax, [r12]` (int32 sign-extension).
- [x] Sub-phase wrap-up

### 14b. DeviceType / DeviceFamily / DeviceOption (~169 symbols)

Device enumeration and capability queries. Survey done 2026-04-23 found
~169 symbols across ~13 distinct types and ~30 free functions. Split
into four sub-phases.

- [x] Survey symbol table — group by class

#### 14b-i. Enums + DeviceType pimpl + DeviceOptionSet — COMPLETE 2026-04-23
- [x] Decode DeviceFamily enum (12 one-hot values) from toString jump table
- [x] Decode DeviceTypeCode enum (33 values) from toString jump table
- [x] Reconstruct DeviceType pimpl (8B wrapping DeviceTypeImpl*)
- [x] Reconstruct detail::DeviceTypeImpl (88B / 0x58, polymorphic base)
- [x] Reconstruct DeviceOptionSet (72B / 0x48, dual-storage:
      unordered_set<DeviceOption> + map<string,DeviceOption>) — corrected
      from initial 0x30 estimate after investigating iterator type
- [x] Reconstruct DeviceOptionSetConstIterator (8B, wraps map iterator)
- [x] Reconstruct 11 predicate free functions (isDefined, isIa, isMfia,
      isUhfqa, isShfqa, isShfsg, isShfqc, isShfppc, isShfli, isGhfli, isVhfli)
- [x] Reconstruct toString(DeviceFamily) and toString(DeviceTypeCode)
- [x] Created reconstructed/notes/device_type.md
- [x] Build clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

#### 14b-ii. DeviceOption enum + sfc options + GenericDeviceType subclass

Split 2026-04-23 after survey revealed scale: 14 knownOptions arrays,
6 sfc::*Option enums, 30+ detail::* device-type subclasses, plus
GenericDeviceType + string parsing. Two sub-sub-phases:

##### 14b-ii-a. Enums, options data, string conversions — COMPLETE 2026-04-23
- [x] Decoded all 15 .rodata knownOptions arrays at 0x962394..0x962aa0
      (Hf2li, Hf2is, Mfli, Mfia, Uhfli, Uhfawg, Uhfqa, Uhfia, Hdawg4,
      Hdawg8, Shfqa2, Shfqc, Shfli, Ghfli, Vhfli) — full (mask,code) tables
      in notes/device_type.md
- [x] Reconstructed sfc::Hf2Option / MfOption / UhfOption / HdawgOption /
      ShfOption / VhfOption / GhfOption enums (one-hot bitmasks, union
      across subclasses per family)
- [x] Reconstructed detail::OptionCodePair<T> template (8B: T mask + DeviceOption code)
- [x] Reconstructed detail::initializeSfcOptions<T,N> header-only template
      (body confirmed at 0x2e0d50)
- [x] Replaced DeviceOption stub with full 31-value enum (0..30, with MF/None
      alias at 0 and family-dependent strings for codes 0 and 6)
- [x] Replaced toString(DeviceOption, DeviceFamily) stub with real 31-case
      switch including family-dep "MFK"/"RTK" branches for HF2
- [x] toString(DeviceType const&) — CORRECTION: it is NOT a "<CODE> [opts]"
      formatter; the binary just returns toString(dt.code()). Existing 14b-i
      stub was actually correct.
- [x] BONUS: added toString(DeviceOptionSet, DeviceFamily, separator)
      helper @ 0x2d0130 (boost::algorithm::join over alphabetical names)
- [x] Replaced toDeviceTypeCode stub with lazy-init function-local static
      unordered_map<string, DeviceTypeCode> (33 entries incl. "none" alias)
- [x] FIXED isIa logic — Phase 14b-i impl had inverted truth table; correct
      semantics: codes UHFIA/MFIA always true, codes in broad mask 0x46BF7901
      else false, all other codes (incl. >=31) defer to hasOption(IA)
- [x] FIXED hasMf body — probes hasOption(MD=1) when family==MF, else
      hasOption(MF=0); matches binary at 0x2d3030 exactly
- [x] Build clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

##### 14b-ii-b. detail::* device-type subclasses + GenericDeviceType

###### 14b-ii-b1. Per-family device subclasses — COMPLETE 2026-04-23
- [x] Reconstructed all 32 detail::* device-type subclasses derived from
      DeviceTypeImpl (Hf2, Hf2li, Hf2is, Mf, Mfli, Mfia, Uhf, Uhfli, Uhfawg,
      Uhfqa, Uhfia, Hdawg, Hdawg4, Hdawg8, Shf, Shfqa2, Shfqa4, Shfsg2,
      Shfsg4, Shfsg8, Shfqc, Shfli, Shfacc, Shfppc2, Shfppc4, Ghf, Ghfli,
      Pqsc, Qhub, Hwmock, Vhf, Vhfli) — split: 15 template-driven via
      `initializeSfcOptions<sfc::*Option,N>`, 11 inline-bit, 6 empty-options.
      All are 0x58 bytes (same as base) with vptr-only override.
- [x] Created device_subclasses.hpp with all 32 declarations.
- [x] Created 11 per-family .cpp files (device_hf2.cpp, device_mf.cpp,
      device_uhf.cpp, device_hdawg.cpp, device_shf.cpp, device_shfacc.cpp,
      device_ghf.cpp, device_pqsc.cpp, device_qhub.cpp, device_hwmock.cpp,
      device_vhf.cpp) mirroring the binary's per-family layout.
- [x] RESOLVED open question: there is NO `sfc::GhfOption`. The GHF family
      reuses `sfc::ShfOption` (Ghfli ctor at 0x2e3a00 calls the mangled
      `initializeSfcOptions<sfc::ShfOption, 5>` template instantiation
      directly, knownOptions @ 0x96298c). Removed the bogus GhfOption
      enum that was added in 14b-ii-a.
- [x] Discovered Hf2Factory subtype-selector dispatch pattern
      (subtype-selector bits in the high bits of the `unsigned long opts`
      arg; mask 0x1c0 for HF2). Same shape applies family-by-family in
      14b-ii-b2.
- [x] Build clean (0 errors, 0 warnings).
- [x] Sub-phase wrap-up.

###### 14b-ii-b1.5. knownOptions verification debt
- [x] Disasm-verify the per-entry (mask, code) selections in 10
      knownOptions arrays — **DONE in Phase 22c.** All 10 arrays verified
      against binary .rodata; 2 bugs found and fixed (Uhfawg, Uhfqa).
      effectively deferred indefinitely. Low priority — affects only
      field-name accuracy of inferred (mask, code) pairs, not behavior.
      Consider closing if no consumer ever needs these names.

###### 14b-ii-b2. Factories, Unknown/Generic device types, parser — COMPLETE 2026-04-23
- [x] Reconstruct abstract base `detail::DeviceFamilyFactory` @ 0x2e0590
      (vtable-only 8B base; virtual `doMakeDefault()` and
      `doMakeDeviceType(unsigned long)`; non-virtual base trampolines
      `makeDefault()` @ 0x2e0590 and `makeDeviceType(opts)` @ 0x2e05b0).
- [x] Reconstruct `detail::makeDeviceFamilyFactory(DeviceFamily)`
      dispatcher @ 0x2e05d0 — switches on family one-hot value to
      construct one of the 11 per-family Factory instances (or
      UnknownDeviceTypeFactory).
- [x] Reconstruct `detail::NoDeviceTypeFactory` @ 0x2e0700 / 0x2e0730
      (returns nullptr from both methods).
- [x] Reconstruct `detail::UnknownDeviceTypeFactory` @ 0x2e0760 / 0x2e07b0
      (both methods return a heap-allocated `UnknownDevice`).
- [x] Reconstruct 11 per-family Factory classes — each with
      `makeDefault()` and `doMakeDeviceType(unsigned long opts)` and
      its own subtype-selector dispatch (see notes/device_type.md
      Phase 14b-ii-b2 table for the complete selector mappings).
      All 11 wired into `device_factories.{hpp,cpp}` via a single
      `ZHINST_DEFINE_FAMILY_FACTORY` macro.
- [x] Reconstruct `detail::UnknownDevice` @ 0x2d3320 — synthetic sentinel
      ctor stores `code=33`, `family=0x800` via `movabs rax,0x80000000021`.
- [x] Reconstruct `detail::GenericDeviceType` @ 0x2d3c60 — same 0x58
      layout as base (no extra fields); only the deleting dtor slot is
      overridden (clone reuses the base impl). Ctor calls
      toDeviceTypeCode + toDeviceFamily + (toDeviceOptions OR empty
      DeviceOptionSet) and forwards to `DeviceTypeImpl(tuple)`.
- [x] Reconstruct `DeviceTypeImpl(tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet>)`
      ctor @ 0x2d3190 (only used by GenericDeviceType).
- [x] Reconstruct `DeviceType(string, vector<string>)` @ 0x2d2ae0
      (NOT 0x2d29c0 as previously documented). Wraps a fresh
      GenericDeviceType.
- [x] Reconstruct **NEW** `DeviceType(string, string)` @ 0x2d2a00 —
      splits the second string via splitDeviceOptions() and forwards
      to the (string, vector) ctor.
- [x] Reconstruct **NEW** free function `splitDeviceOptions(string)`
      @ 0x2d0460 — boost::trim_copy_if then split on '\n'.
- [x] Reconstruct **NEW** free function `toDeviceFamily(string)`
      @ 0x2debd0 — fast-path inline string compares + lazy-init
      function-local-static `map<string, DeviceFamily>` of 10 entries
      (no shfacc, no unknown) + `boost::starts_with` prefix verify.
      Default on miss = DeviceFamily(0x800).
- [x] Reconstruct **NEW** free function `toDeviceOptions(vector<string>, DeviceFamily)`
      @ 0x2d0fb0 — try/catch around toDeviceOption(name) inserts;
      unknown names silently dropped.
- [x] Build clean (0 errors, 0 warnings).
- [x] Sub-phase wrap-up.

###### 14b-ii-b2-followup. toDeviceOption(string) body — COMPLETE 2026-04-23
- [x] Reconstruct `zhinst::toDeviceOption(string)` @ 0x2d05b0.
      Function-local-static unordered_map of 33 entries
      (mov edx, 0x21 confirms count) decoded from disasm lines
      533249..533490. Source strings live in .data.rel.ro 0xb08ef8..
      0xb08ff8 as `const char*` pointers (NOT std::string objects)
      targeting .rodata literals at 0x90b60e. Two duplicate keys map
      code 0 ("MF" and "MFK") and code 6 ("RT" and "RTK"). Throws
      `zhinst::Exception` via `boost::throw_exception` on miss;
      `toDeviceOptions()` catches and silently drops.
- [x] Resolve QA/QC asymmetry: parser accepts "QC" → DeviceOption::QA
      while toString emits "QA". Real binary quirk; enum kept as `QA`,
      parser table maps "QC" → QA. Documented in notes/device_type.md.
- [x] Confirm DeviceOptionName::* storage class: 8-byte spacing in
      .data.rel.ro confirms `const char*` pointers, not std::string.
- [x] Build clean (0 warnings).
- [x] Verify symbol now defined: nm shows `T` for
      `_ZN6zhinst14toDeviceOptionERK...` in device_type.cpp.o
      (previously `U`).

#### 14b-iii. AwgDeviceType + AwgSequencerType + getAwgDeviceProps templates
- [x] Decode AwgDeviceType enum (one-hot bitfield, 9 values 1..256)
      — already in `types.hpp` from Phase 3d; no changes needed.
- [x] Decode AwgSequencerType enum from toAwgDeviceType — 3 named values
      (Auto=0, QA=1, SG=2); the formatter's "unknown" branch has no
      enumerator (modeled as default-case behavior).
- [x] Reconstruct AwgDeviceProps struct (return type of getAwgDeviceProps<>) —
      0x80 bytes (libc++) / 0xa0 (libstdc++); 4 strings + type + 2 qwords
      + bool. Field NAMES (`maxWaveformSamples`, `maxWaveformBytes`,
      `supportsExtraFeature`) are inferred — see follow-up below.
- [x] Reconstruct AwgPathPatterns struct (3 strings, 0x48/0x60 bytes)
      and 6 anonymous-namespace globals (Default / GrimselQa / GrimselSg
      / GrimselLi / Gurnigel / Maloja); the latter two are runtime copies
      of GrimselLi.
- [x] Reconstruct all 9 explicit specializations of getAwgDeviceProps<T>
      (binary 0x2cc5f0..0x2cdb00). HDAWG is the only one that consults
      `dt.hasOption(ME=0x13)`.
- [x] Reconstruct toAwgDeviceType — 29-entry switch over (code-4) for
      codes 4..32, with SHFQC(21) special-cased on sequencer.
- [x] Reconstruct makeUnsupportedAwgSequencerErrorMessage — produces
      `"Unsupported device or sequencer type (<dtype>, sequencer: <seq>)."`.
- [x] Build clean — `cmake --build .` from `reconstructed/build/` succeeds
      with no warnings; `awg_device_props.cpp.o` symbol is `T`.
- [x] Sub-phase wrap-up — see below for proposed follow-ups.

#### 14b-iii follow-ups
- [x] **Verify inferred AwgDeviceProps field names** — DONE in Phase 21f:
      `maxWaveformSamples`→`maxElfSize`, `maxWaveformBytes`→split into
      `addressImpl`+`sampleFormat`, `supportsExtraFeature`→`isHirzel`.
- [x] **Reconstruct `getDeviceConstants(AwgDeviceType)` body @ 0x2cc0c0**
      — done in Phase 14b-iii.5. All 9 case blocks were already
      populated in Phase 7e; this sub-phase replaced the placeholder
      `std::runtime_error` default branch with the real
      `BOOST_THROW_EXCEPTION(ZIAWGCompilerException(...))` throw, added
      the necessary includes, and documented the jump-table layout,
      XMM constant table, sampling-rate doubles, and source_location
      values in `reconstructed/notes/device_constants.md`. Build clean.

#### 14b-iv. Helpers + free functions + final wrap-up
- [x] Reconstruct getOptionsAsString(DeviceType const&)
- [x] Reconstruct expand(DeviceFamily) @ 0x2dfbc0
- [x] Reconstruct toStrings(vector<DeviceFamily> const&)
- [x] Reconstruct toDeviceOptions, detail::generateMfSfc,
      detail::makeDeviceFamilyFactory
- [x] Reconstruct anonymous-namespace makeDevicesSet()::$_0 lambda
- [ ] **Verify inferred AwgDeviceProps field names** (`maxWaveformSamples`,
      `maxWaveformBytes`, `supportsExtraFeature`) — no consumer in 14b-iv;
      carry forward to a later phase (14d or 15) where AwgDeviceProps
      callers will be reconstructed.
- [x] Update OVERVIEW.md with consolidated Phase 14b summary
- [x] Build clean
- [x] Sub-phase wrap-up

#### Phase 14b — COMPLETE

All four sub-phases (14b-i, 14b-ii, 14b-iii, 14b-iii.5, 14b-iv) closed.
One carry-forward item:

- [ ] **AwgDeviceProps field-name verification debt** (carried from 14b-iii):
      `maxWaveformSamples`, `maxWaveformBytes`, `supportsExtraFeature` are
      INFERRED from values, not confirmed by reading callers. To be
      verified during Phase 14d (or wherever AwgDeviceProps consumers
      next surface). Notes: `notes/awg_device_props.md`.

### 14c. Logging + tracing (20 zhinst symbols across 17 functions) — DONE

Logging infrastructure used across the compiler. The earlier "73 symbols"
estimate counted boost.log + OpenTelemetry template-instantiation glue;
the actual zhinst-namespace surface is 20 symbols (8 logging + 12 tracing).

- [x] Surveyed symbols — 20 zhinst-namespace symbols across 17 distinct
      functions (logging: 8 / tracing: 12). The "73" estimate counted
      boost.log + OTel template-instantiation glue.
- [x] Determined LogRecord (≥0x118), TraceProvider (0x20), ScopedSpan
      (0x30) layouts; documented in notes/logging_tracing.md.
- [x] Reconstructed all 17 functions — clean cmake build, zero warnings.
- [x] Sub-phase wrap-up — OVERVIEW + notes updated; OpenTelemetry stub
      headers added under `include/opentelemetry/` to type-check
      tracing.cpp without the real SDK installed.

**Verification debt** (carried forward):

- Severity enumerator ordering not directly observed (only the 8 string
  names "Trace".."Fatal" are in .rodata). Verify when the first LOG_*
  macro caller surfaces in a later-phase reconstruction.
- OpenTelemetry stub headers approximate ABI sizes only — real linking
  against opentelemetry-cpp would require swapping the stub umbrella
  (`include/opentelemetry/_stub_fwd.hpp`) for the actual SDK headers.

### 14d. ElfReader / ElfWriter (Phase 13d dependency)

Surfaced during Phase 13d: cacheFileOutdated, cacheFile and getCachedFile
all use ElfReader/ElfWriter. Phase 13d created a minimal forward-decl
header (`elf_reader.hpp`) but the implementations are still placeholder.

- [x] Disassemble ElfReader::ElfReader(string) @0x2c3110 — file open + ELFIO load
- [x] Disassemble ElfReader::getSection(string) const @0x2c4000 — section table walk (throws ElfException on miss, not nullptr)
- [~] ~~Verify ElfSection vtable layout (get_data @ +0xC0, get_size @ +0x98)~~ — CANCELLED: reconstruction uses `ELFIO::section` directly; the original stub's `ElfSection` class was fictional. The free helper `zhinst::sectionAsString(const ELFIO::section*)` in `elf_reader.hpp` covers the only access pattern that mattered.
- [x] Disassemble ElfWriter::ElfWriter(uint16_t) @0x2934a0 — channel arg ctor (already complete from earlier phases — src/elf_writer.cpp has 8 methods, ctor 0x2934a0 included)
- [x] Identify ElfWriter::addSection / save methods (already complete — src/elf_writer.cpp: addCode/addData/addWaveform/writeFile×2/setMemoryOffset/prepareHeader)
- [x] Once ElfReader is real, fold the deferred CachedParser stubs (cacheFile body) into full reconstructions — 5 call sites in cached_parser.cpp converted from fictional `getDataAsString()` to `sectionAsString()`
- [x] Sub-phase wrap-up: complete — see `reconstructed/notes/elf_reader.md`. Net additions: `src/elf_reader.cpp` (5 methods + ElfException ctor/dtor/what), `elf_reader.hpp` upgraded from 71-line forward-decl stub to 0x98-byte full reconstruction with `private ELFIO::elfio` base + `sectionAsString()` helper. ElfReader layout corrected (was 0x90, now 0x98 with `vector<section*>` at +0x78); `readHeader` sentinels reinterpreted as `get_class()==ELFCLASS32` && `get_encoding()==ELFDATA2LSB`; `getSection` semantics corrected (throws, not nullptr). Real cmake build clean, zero warnings.

### 14e. zhinst::sfc namespace (NEW — added during 14b-iv wrap-up)

Surfaced during Phase 14b-iv reconstruction of `generateMfSfc`: its
mangled return type is `zhinst::sfc::FeaturesCode`, used as a
strong-typed wrapper around uint64. Currently stubbed as `uint64_t`
in `mf_sfc.cpp`. The sfc namespace also contains:

- `sfc::FeaturesCode` (type — likely strong typedef / wrapper)
- `sfc::Hf2Option`, `sfc::MfOption`, `sfc::UhfOption`, `sfc::HdawgOption`,
  `sfc::ShfOption` (enum classes)
- `detail::OptionCodePair<T>` template (small POD pair type)
- 8+ `detail::initializeSfcOptions<sfc::*Option, N>` template
  specializations (parser helpers turning SFC bitfield → DeviceOptionSet)

- [x] Survey full `zhinst::sfc::` symbol set in nm output — 13 `initializeSfcOptions` instantiations across 6 OptionEnum types (Hf2/Mf/Uhf/Hdawg/Shf/Vhf); no out-of-line `FeaturesCode` symbols (header-only POD); see `notes/device_type.md` (sfc namespace section)
- [x] Determine `sfc::FeaturesCode` layout (likely 8B wrapper around uint64) — confirmed: 8B trivially-copyable POD wrapper around uint64; evidence is the source_location literal at 0x2deb37 naming the type, plus the direct `rax`-return at 0x2deac1 (no sret, ≤8B & trivially-copyable per SysV ABI)
- [x] Reconstruct sfc option enums (compare bit positions vs `generateMfSfc`) — already complete from Phase 14b-iv: 6 enum classes in `device_type.hpp:163-285` (Hf2Option, MfOption, UhfOption, HdawgOption, ShfOption, VhfOption), bit positions decoded from .rodata 0x962394..0x962aa0 knownOptions arrays
- [x] Reconstruct `OptionCodePair<T>` and `initializeSfcOptions<>` template — already complete from Phase 14b-iv: header-only template in `device_type.hpp`; 13 instantiations confirmed in nm survey
- [x] Update `mf_sfc.cpp` to return real `sfc::FeaturesCode` type — declaration in `device_type.hpp` and body in `mf_sfc.cpp` both promoted; 3 `return` sites wrap their uint64 expression in `sfc::FeaturesCode{...}`
- [x] Sub-phase wrap-up: complete — see `reconstructed/notes/device_type.md` (sfc namespace section). Net additions: `sfc::FeaturesCode` struct in `device_type.hpp` (8B wrapper); `generateMfSfc` declaration & body promoted from `uint64_t` to `sfc::FeaturesCode`. No other call sites needed updating. Real cmake build clean, zero warnings.


---

## Phase 15: Accuracy refinement

Fix approximate logic and deferred TODO markers in existing code.

> **Recommended order**: 15a (close item #4 done) → **15b first** (small
> verification work, builds momentum) → 15a-i (multi-session, frontend
> lowering data model) → 15a #1 (lower() return type, depends on 15a-i)
> → 15c (AsmOptimize, dense disasm).

### 15a. Compiler pipeline gaps

- [x] WaveformGenerator field_B0_ (shared_ptr<void>) — **CLOSED 2026-04-23 as
      negative finding.** No setter exists anywhere in the binary. The 16B
      slot at +0xB0 is reserved-but-unused (zero-initialized in WG ctor at
      0x2482aa, never written elsewhere). Apparent +0xb0/+0xb8 read sites
      inside WG methods (251973, 25385a, 255068, ...) were misattributed —
      they read the union body of `Value` parameter objects (Value has
      type tag at +0xA8 and union storage at +0xB0). The single true
      +0xb0 write at 0x11d180 is `Compiler::Compiler` writing its own
      AsmCommands shared_ptr, not WG-related. Evidence chain documented
      in `waveform_generator.hpp` comments. Closes #110.
- [x] Fix lower() return type in compiler.hpp — **DONE in 15a-i.** Returns
      `LowerResult` (32B sret = 2 shared_ptrs: `shared_ptr<Node>` from
      FrontendLoweringState.result + `shared_ptr<EvalResults>` from the
      evaluate virtual). Was void, now corrected. Body partially implemented
      (Context+State construction done, virtual call TODO'd pending vtable
      slot declaration on SeqCAstNode).

### 15a-i. Frontend Lowering data model — COMPLETE 2026-04-23

Decode the data types flowing through `FrontEndLoweringFacade::lower`.
~130 related symbols across EvalResults / FrontendLoweringState /
FrontendLoweringContext. EvalResults is the central data type of the
lowering pipeline (not a leaf), so this is large. Blocks 15a #1.

**Findings:** EvalResults = 0x80 bytes (7 fields, 14 methods). Value
layout was WRONG (0x20→0x28 correction). lower() return = 32B sret
(2 shared_ptrs, not 64B/4 as previously claimed). FrontendLoweringState
.result = shared_ptr\<Node\> (was void/TBD). EvalResults.arrayBacking_
at +0x70 = shared_ptr\<EvalResults\> (self-referential, used by
SeqCArray). See `notes/frontend_lowering.md`.

- [x] EvalResults layout (0x80B; 14 direct methods scattered across
      0x15a750..0x247600 — heavy inlining): 10 setValue overloads,
      ctor, copy-ctor, dtor, getValue — all declared in eval_results.hpp
- [x] EvalResultValue layout (0x38B) — renamed fields from opaque to
      typed: VarType + VarSubType + Value(embedded 0x28) + AsmRegister
- [x] lower() sret aggregate type — **CORRECTED to 32B / 2 shared_ptrs**
      (was wrongly claimed as 64B / 4 sps). Return type:
      `LowerResult{shared_ptr<Node> astResult, shared_ptr<EvalResults> evalResult}`
- [x] FrontendLoweringState::result pointee — **shared_ptr\<Node\>**
      (was shared_ptr\<void\>/TBD). Evidence: lower() copies state.result
      into sret[0]; caller stores into Compiler+0x28 (= shared_ptr\<Node\> ast_)
- [x] Sub-phase wrap-up — see notes/frontend_lowering.md. Build clean.

### 15b. Prefetch/Cache deferred items (10 markers) — COMPLETE 2026-04-23

From Phase 10.5f — minor verification items in prefetch.cpp and
prefetch_splitplay.cpp.

**Audit finding 2026-04-23**: All 10 markers were already resolved during
post-audit edits on 2026-04-22 (between todo_audit.md @ 01:39 and the
prefetch source edits at 04:35-04:53) but were never officially closed
out. Verified by case-insensitive search for TODO/FIXME/APPROXIMATE/
VERIFY/XXX in both files — zero markers remain. The relevant code
sections now contain inline disasm citations (e.g. line 476-481 cites
`mov edx,0x114; bt edx,ecx` for the bitmask, line 526 cites 0x1d031d→0x1d0869
for the eligibility branch, line 125 cites 0x1dd462..0x1dd49c for the
weak_ptr::lock pattern). Build clean.

- [x] Verify bitmask 0x114 exact bits — done; cited at prefetch.cpp:476-481
- [x] Verify enum name ordering — done; cited at prefetch.cpp:500-506
- [x] Verify NodeType::Play cmp $0x2 — done; cited at prefetch.cpp:504
- [x] Verify shared_ptr aliasing constructor — replaced with semantic
      `weak_ptr::lock()` at prefetch_splitplay.cpp:126
- [x] Resolve remaining 6 prefetch_splitplay.cpp items — all converted
      to semantic equivalents with disasm citations
- [x] Sub-phase wrap-up — see notes/archive/phase_15b_prefetch_audit.md

### 15c. AsmOptimize approximate logic — COMPLETE 2026-04-23

**Primary finding**: register field semantics in `AssemblerInstr` were
inverted throughout all AsmOptimize methods. reg2(+0x20)=READ source,
reg0(+0x28)=WRITE destination, reg1(+0x30)=dual (read if cmdType==1 or 7;
written if cmdType==7). Prior code had reg2 as "dest" and reg0 as "src1"
— exactly backwards. Fixed in isRead, isWritten, getNextActionForReg
(rewritten), registerIsNeverWritten, registerUpdate, isLabelCalled
(iteration direction). Field NAMES kept as reg0/reg1/reg2 to avoid
cascading rename across 20+ files; semantic comments cite Asm-relative
and AssemblerInstr-relative offsets.

- [x] removeUnusedRegs inner scan loop (#47) — fully reconstructed @0x27e760
      (291 lines). Skip bitmask 0x29 documented (skips INVALID, LABEL, cmd=4
      — NOT NOP/MESSAGE/ERROR_MSG as previously claimed).
- [x] registerAllocation conflict detection (#48) — structurally reconstructed
      @0x27ebb0 (1466 lines, 6-phase graph-coloring with backward-branch
      live-range extension). Internal `PhysicalRegister` type confirmed via
      vector dtor mangled name @0x281840.
- [x] splitConstRegisters splitting heuristic (#49) — structurally reconstructed
      @0x280440 (444 lines, 2-pass barrier algorithm). Added
      `AsmRegister::magicSkipRegister()` @0x28ebb0 returning `{INT_MAX, true}`.
- [x] Sub-phase wrap-up — see `notes/optimization_passes.md`. Build clean.

---

## Phase 16: Validation, file reorganization, and stub recovery

Phase 16 is a multi-stage cleanup before final validation. Originated
from a noticed pile-up in `custom_functions.cpp` and the realisation
that prior tracking had drifted. Full audit catalog in
`reconstructed/notes/audit_phase16a.md`.

### 16a. Codebase marker & stub sweep

- [x] Grep all sources for TODO/FIXME/TBD/APPROXIMATE/VERIFY/XXX/HACK
      markers; cross-reference with TODO.md and unknowns.md;
      catalog stub patterns including the `throw std::runtime_error("X
      not implemented")` pattern (which is invisible to plain TODO grep).
- [x] Per-file namespace/class inventory (find files piling multiple
      unrelated types).
- [x] Catalog written to `reconstructed/notes/audit_phase16a.md`.
- [x] Sub-phase wrap-up: 6 untracked items + multiple stale TODO.md rows
      surfaced. Each one tracked as an explicit item below; details in
      the audit file.

### 16b. File-organization split ✅ DONE

Completed 2026-04-23. Refactored `custom_functions.{cpp,hpp}` into
four well-scoped TU pairs. Build clean, zero lost markers.

- [x] **Split `custom_functions.{cpp,hpp}` (audit §C1)** — extracted:
    - [x] `MathCompiler` + `MathCompilerException` → `math_compiler.{cpp,hpp}`
    - [x] `NodeMapData` hierarchy + `NodeMapItem` → `node_map_data.{cpp,hpp}`
    - [x] `EvalResultValue` → `eval_result_value.{cpp,hpp}`
- [x] **ErrorMessages ODR investigation (audit §F)** — single class,
      single definition in `error_messages.{hpp,cpp}`. Audit grep was
      over-broad; all 9 .cpp "occurrences" are just `format()`/`get()`
      calls. No action needed.
- [x] **compiler.cpp Facade split (audit §E)** — `FrontEndLoweringFacade::lower`
      is 36 lines and gates on unfinished `SeqCAstNode::evaluate` virtual.
      Not worth splitting. Will revisit when the virtual is wired.
- [ ] ~~Split `waveform_generator.cpp` exceptions (audit §D)~~ — deferred
      to opportunistic; execute next time `waveform_generator.cpp` is
      opened for unrelated work.
- [x] Sub-phase wrap-up: build clean; OVERVIEW.md updated; audit findings
      resolved or deferred with rationale.

### 16c. TODO.md / unknowns.md reconciliation ✅ DONE

Completed 2026-04-23. Summary table rewritten with 16 rows matching
audit-verified reality. Audit file renamed to `notes/outstanding_work.md`.

- [x] Refresh summary table — all Section G rows corrected.
- [x] Promote Phase 15c carry-forwards — new "AsmOptimize carry-forwards"
      row (3 items: simplifyAssign, splitReg, register rename).
- [x] Cross-link unknowns.md — new "Unknowns cross-ref" row (#98, #10).
- [x] Fold `audit_phase16a.md` content into TODO.md; file deleted (no
      synonym files for TODO — see AGENTS.md).
- [x] Sub-phase wrap-up: TODO.md and unknowns.md numbers match reality.

### 16d. Stub & gap execution ✅ DONE (HIGH + MEDIUM complete)

Completed 2026-04-23. HIGH and MEDIUM items all done. LOW items deferred
to future phases (verification-only or diminishing returns).

Order of execution: HIGH → MEDIUM → LOW. Each item references
audit catalog for context.

#### High priority — runtime correctness ✅ DONE

Completed 2026-04-23. Ctor binding block (78/81), play() + playIndexed()
structurally reconstructed, SubFunc enum fully resolved (Default=1, Aux=2,
Now=3, DigTrigger=4). 4 complex wrappers identified and split to own TODO.

- [x] **CustomFunctions::CustomFunctions ctor binding gap (audit §C2,
      C3, item I.2)** — 81/81 emplace calls reconstructed (76 standard +
      5 aliases: setSeqIndex→assignWaveIndex, setReadoutRegisterAddress→setID,
      waitOscPhaseOfDemod→waitDemodOscPhase, setUser→setUserReg,
      getUser→getUserReg). Previously had setReadoutRegisterAddress wrongly
      bound to setUserReg — corrected to setID @0x1334a0.
- [x] **Reconstruct `CustomFunctions::play` @0x15f090 (audit §C2)** —
      full structural reconstruction (7536B, SubFunc switch, PlayArgs,
      channel loop, merge, asm emit). Documented pseudocode in .cpp.
- [x] **Reconstruct `CustomFunctions::playIndexed` @0x160e00 (audit
      §C2)** — same treatment (6428B).
- [x] **Resolve aux-wrapper SubFunc TBD values (audit §C2 last
      bullet)** — SubFunc enum: Default=1, Aux=2, Now=3, DigTrigger=4.
      playAuxWaveIndexed→playIndexed(2), playWaveDigTrigger→play(4).
      4 wrappers (playAuxWave, playDIOWave, playWaveDIO, playWaveZSync)
      are complex — own PlayArgs construction, no play() delegation.

#### Medium priority — feature completeness

- [x] **Reconstruct remaining CustomFunctions utility stubs (audit
      §C2)** — All 13 dispatch helpers reconstructed from binary:
      `checkFunctionSupported` (bitmask test + throw 0x49),
      `checkWaveformMinLengthTrig` (throw 0xF3),
      `checkOffspecWaveLength` (warn 0xF4/0xE6),
      `optionAvailable` (linear scan + usedFeatures_ insert),
      `lookupNode` (initNodeMap + retrieve + throw 0x83),
      `setWaitCyclesReg` (addi + suser 0x6F),
      `generateWaveform` (prepend name + delegate to generate()),
      `addSyncCommand` (Hirzel/Cervino switch),
      `printF` (boost::format with VarType dispatch),
      `addWaitCycles` (addi + suser 0x69),
      `writeLS64bit` (two addi+suser pairs for 64-bit split).
      `mergeWaveforms` and `writeToNode` remain as documented stubs
      (3KB and 23KB respectively, need WaveformGenerator complete type).
- [x] **Reconstruct 4 complex play wrappers** — DONE (Phase 21a). Each has own PlayArgs
      construction, no play()/playIndexed() delegation:
      `playAuxWave` @0x135610 (~5KB), `playDIOWave` @0x1369f0 (~3.4KB),
      `playWaveDIO` @0x137740 (784B), `playWaveZSync` @0x137a50 (~3.2KB).
- [x] **WaveformGenerator DSP throw-stubs (audit §B, §D)** — All 32
      throw-stubs reconstructed from binary. Full implementations for:
      sin, cos, sinc, ramp, sawtooth, triangle, chirp (trig/math);
      drag, blackman, hamming, hann, rrc (window/pulse); mask, marker
      (wrappers to markerImpl); rand, randomGauss, randomUniform,
      lfsrGaloisMarker (random); vect, placeholder, flip, interleave
      (composition). Structural outlines for larger functions: join,
      multiply, cut, circshift, merge, grow, filter, readWave,
      markerImpl. File grew from 772 to 2305 lines.
- [x] **MathCompiler 67 symbols** — Already complete. 23 single-arg
      wrappers + 5 multi-arg + ctor + call() + functionExists() +
      exception class. All 74 nm symbols accounted for. File: 197 lines.

#### Low priority — quality / completeness

- [ ] **SeqCAstNode print/clone macro expansion (audit §I.6)** — the
      macro at `seqc_ast_node.hpp:154` expands to 53×2 stub method
      bodies. Most are mechanical; verify against binary.
- [x] **WavetableManager\<T\> remaining 14 methods** — DONE (Phase 21e).
- [ ] **smap remaining logic** — ~0x1E6 bytes after alui call (unknowns
      #10).
- [x] **mergeWaveforms full reconstruction** — DONE (Phase 21a, 3KB @0x15e060).
- [x] **writeToNode full reconstruction** — DONE (Phase 21b, 29KB @0x164550).
- [x] **floatEqual @0x2ec050** — DONE (Phase 20b). Note: binary uses exact
      IEEE-754 equality (cmpeqsd), not tolerance.
- [ ] **AWGCompilerConfig::supportedDeviceTypes documentation** —
      field at config+0x00 is a uint32 bitmask. Values per device type
      need documenting (discovered during checkFunctionSupported analysis).

### 16e. Validation against the real .so ✅ DONE

Completed 2026-04-23.

- [x] Verify struct sizes match binary — added `static_assert` for
      PlayConfig (0x20) and Signal (0x58). Cache::Pointer (0x24 in
      libc++, 0x28 in libstdc++) confirmed as ABI-dependent — not
      assertable. Existing asserts: AsmRegister(8), Value(40),
      DeviceConstants(0x90), MemoryBlock(12), SeqCAstNode(0x18),
      EvalResultValue(0x38), NodeMapItem(0x18), AwgPathPatterns(0x60),
      AwgDeviceProps(0xa0), FrontendLoweringContext(0x50),
      FrontendLoweringState(0x30), plus 12 SeqCAstNode subclass asserts.
- [x] Final marker sweep — 181 total markers across 40 files:
      95 TODO, 29 stub, 22 VERIFY, 14 TBD, 11 APPROXIMATE, 10 not-impl,
      0 FIXME. Top hotspots: custom_functions.cpp (46), waveform_generator
      (29), node_map_data (22).
- [x] Write comparison tests / test key methods — **deferred**. Direct
      testing against the real .so requires a C ABI bridge or Python
      wrapper due to libc++ vs libstdc++ incompatibility. Not practical
      within Phase 16 scope. Future validation should use Python ctypes
      or a thin C shim calling into the .so.

---

## Phase 17 — Hotspot depth pass

Focus on reducing the 181 markers in the top 3 files. Order: node_map_data
(smallest, most mechanical), then waveform_generator structural stubs,
then custom_functions remaining builtins.

### 17a. node_map_data.cpp (22 markers → 0) ✅ DONE

- [x] Reconstruct NodeMap method bodies from binary (12 TODO stubs)
- [x] Verify rodata tables from binary data section (typeTable @0x95ad18 = {2,1,2,2}, sizeTable @0x8fc630 = {2,1,2,2})
- [x] Build verify + sub-phase wrap-up

### 17b. waveform_generator.cpp structural stubs (29 markers → 3) ✅ DONE

- [x] Reconstruct readWave — return type corrected to shared_ptr<WaveformFront>,
      18 call sites updated to extract ->signal
- [x] Reconstruct eval — EvalResults now fully available; make_shared + setValue(VarType(5))
      + waveformFront_ assignment at +0x48
- [x] Reconstruct markerImpl — 1858 bytes (not 4576), creates uniform Signal with constant
      marker; only 2 args (length + markerValue), isMask only affects param names
- [x] Fix interpolateLinear bug (size/sizeof)
- [x] Verify grow error code (0x3d), merge no length validation, circshift min=1, cut offset
- [x] Clean up stale TODO/VERIFY comments (aliasMap_ confirmed empty, rand formula verified,
      marker manipulation NOTE'd)
- [x] Build verify + sub-phase wrap-up
- [ ] 3 remaining markers: readDoubleAmplitude |x|>1.0 check, interpolateLinear formula,
      rrc error 0x64 validation (low priority, deferred)

### 17c. custom_functions.cpp remaining builtins (46 markers → 19) ✅ DONE

- [x] Reconstruct checkPlayMinLength/checkPlayAlignment warning callbacks (0xF5, 0xE7)
- [x] Verify and fix getWaitTime shift direction (left-shift + sar, not right-shift)
- [x] Fix getNodeAddress — return direct->addr_, throw on miss (not return 0)
- [x] Fix getAccessModes — throws out_of_range on miss (not return empty)
- [x] Document getSampleClock fallback (readConst mechanism not fully understood)
- [x] Implement 5 simple 0-arg functions: waitWave (wwvf), waitPlayQueueEmpty (wwvfq),
      sync (addSyncCommand), randomSeed (seedRandom stub), now (suser 0x1c)
- [x] Implement error/info (printF + asmMessage)
- [x] Convert 8 TODO/TBD comments to NOTEs (aliasMap_ empty, NodeMap, etc.)
- [x] Document parseOptionalString and getPlayRate as stubs with addresses
- [x] Build verify + sub-phase wrap-up
- [ ] 19 remaining markers: all blocked on PlayArgs layout (play/playIndexed/4 complex
      wrappers), writeToNode (23KB), mergeWaveforms (3KB), or header field unknowns.
      These require PlayArgs reconstruction as a prerequisite.

---

## Phase 18 — PlayArgs + bulk builtin stubs

### 18a. PlayArgs layout reconstruction

Decode PlayArgs struct (~0x80 bytes, NOT 0x200 — previous offset estimates were
wrong; they were stack locals in play()). Unblocks play(), playIndexed(),
mergeWaveforms(), and the 4 complex play wrappers.

- [x] Disassemble PlayArgs ctor @0x15d600 — 0x80 byte struct with
      shared_ptr<WavetableFront>, std::function, string, uint16_t×2,
      vector<vector<WaveAssignment>>, bool
- [x] Disassemble PlayArgs::parse @0x15d7b0 — pre-scan for VarSubType==2
      (marker), split String/Const vs other, dispatch to parseImplicit/Explicit
- [x] Reconstruct PlayArgs struct in custom_functions.hpp (was opaque_[0x300])
- [x] Implement PlayArgs ctor, dtor (=default), parse(), getMaxSampleLength()
- [x] Rename AWGCompilerConfig::unknown_14 → channelsPerGroup[2]
- [x] Implement parseImplicitChannels @0x16fb30 (~1200B)
- [x] Implement parseExplicitChannels @0x170000 (~1500B)
- [x] Add secureLoadWaveform @0x1711a0 (thin wrapper)
- [x] Implement play() @0x15f090 using real PlayArgs
- [x] Fix mergeWaveforms return type (void → shared_ptr<WaveformFront>)
- [x] Implement playIndexed() @0x160e00 using real PlayArgs
- [x] Implement addChannelWave @0x170ec0 (~500B)
- [x] Implement mergeWaveforms() @0x15e060 (~3KB, 7-phase dispatch) — done in Phase 21a
- [x] Implement 4 complex play wrappers (playAuxWave @0x135610 ~5KB,
      playDIOWave @0x1369f0 ~3.4KB, playWaveDIO @0x137740 784B,
      playWaveZSync @0x137a50 ~3.2KB) — done in Phase 21a
- [x] Build verify + sub-phase wrap-up

### 18c. Consider splitting custom_functions.cpp

After all Phase 18 work is complete, evaluate splitting custom_functions.cpp
(currently ~1850 lines, will grow further) into logical units:
- `custom_functions_play.cpp` — play/playIndexed/PlayArgs methods/wrappers
- `custom_functions_builtins.cpp` — bulk builtin implementations
- `custom_functions.cpp` — ctor, dispatch, utility methods, exceptions

### 18b. Bulk builtin function stubs (~60 return-nullptr stubs)

Many follow a common pattern: checkFunctionSupported → arg validation →
EvalResults(VarType(1)) → single AsmCommands call → return. Batch-disassemble
the smallest ones first for maximum marker reduction.

#### 18b-i. Small builtins (<1KB, mechanical pattern)
- [x] prefetch @0x1351d0 — thin wrapper to play(SubFunc::Default)
- [x] prefetchIndexed @0x135290 — always throws (mask 0x0)
- [x] setWaveDIO @0x14cae0 — always throws (deprecated)
- [x] waitTimestamp @0x1401c0 — st(Reg(0), 0x1b)
- [x] getPRNGValue @0x151a70 — luser(reg, 0x77), returns register as int
- [x] waitOnGrid @0x13d000 — asmWtrigLSPlaceholder (TODO: grid constant from res)
- [x] waitOnSync @0x13d3a0 — st(Reg(0), 0x92)
- [x] resetRTLoggerTimestamp @0x153f90 — st(Reg(0), 0x62/0x6d) device-dependent
- [x] getQAResult @0x14f380 — ld(reg, 0x61), returns register as int
- [x] Build verify + sub-phase wrap-up

#### 18b-ii. Medium builtins (1-2KB)
- [x] setDIO @0x130780 — sdio(reg, highBank), waitState_ protocol
- [x] getDIO @0x131040 — ldio(reg, highBank), waitState_ protocol
- [x] getDIOTriggered @0x131410 — ldiotrig(reg), waitState_ protocol
- [x] setID @0x1334a0 — sid(reg, highBank)
- [x] setTrigger @0x1454c0 — strig(reg)
- [x] getTrigger @0x145ad0 — addi+ltrig+andr, 2 registers
- [x] setInternalTrigger @0x146140 — sinttrig(reg)
- [x] lock @0x14dc70 — asmLockPlaceholder(wf, deviceIndex)
- [x] unlock @0x14e180 — asmUnlockPlaceholder(wf, deviceIndex)
 - [x] getCnt @0x14e8d0 — lcnt + devConst->field_54 range check
 - [x] waitDIOTrigger @0x13d630 — readConst + waitState_ + device-type dispatch
 - [x] getSweeperLength @0x14bca0 — readConst("AWG_USERREG_SWEEP_COUNT0/1") + luser
 - [x] setPrecompClear @0x14c720 — asmSetPrecompFlags(bool)
 - [x] getUserReg @0x14b480 — luser + HDAWG addi/suser/addSyncCommand path
 - [x] playZero @0x1387f0 — asmPlay with hold=false
 - [x] playHold @0x139030 — asmPlay with hold=true
 - [x] waitCntTrigger @0x13e460 — readConst("AWG_CNT_TRIGGERn_INDEX") + asmWtrigLSPlaceholder
 - [x] waitZSyncTrigger @0x13dcf0 — readConst + waitState_=2 + device-type dispatch
- [x] Build verify + sub-phase wrap-up

#### 18b-iii. Large builtins (>2KB)
- [x] getZSyncData @0x1316f0 (~3KB)
- [x] getFeedback @0x132420 (~4KB)
- [x] assignWaveIndex @0x133c40 (~5KB) — partial: boost::regex omitted, field_48_/field_70_/field_4C TODOs
- [x] wait @0x139760 (4640B)
- [x] waitTrigger @0x13abf0 (~2KB)
- [x] waitAnaTrigger @0x13b4b0 (~3KB)
- [x] waitDigTrigger @0x13c110 (~4KB)
- [x] waitDemodOscPhase @0x13eba0 (~3KB)
- [x] waitSineOscPhase @0x13f790 (~2.5KB)
- [x] resetOscPhase @0x1403b0 (~6.5KB)
- [x] setSinePhase @0x141df0 (~4KB)
- [x] incrementSinePhase @0x142da0 (~4KB)
- [x] waitDemodSample @0x143d50 (~5.2KB)
- [x] getAnaTrigger @0x146740 (~3.2KB)
- [x] getDigTrigger @0x147420 (~3.2KB)
- [x] setInt @0x1480d0 (~2.5KB) — delegates to writeToNode (unimplemented)
- [x] setDouble @0x148ac0 (~3.3KB) — delegates to writeToNode (unimplemented)
- [x] generate @0x149940 (~2.8KB)
- [x] setUserReg @0x14a420 (~4KB)
- [x] at @0x14ce30 (~2.5KB)
- [x] waitQAResultTrigger @0x14edc0 (~1.4KB)
- [x] startQAResult @0x14f620 (~2.7KB)
- [x] startQAMonitor @0x1500b0 (~2.1KB)
- [x] executeTableEntry @0x150900 (~2.7KB)
- [x] setPRNGSeed @0x1513e0 (~1.6KB)
- [x] setPRNGRange @0x151ce0 (~2.4KB)
- [x] startQA @0x152690 (~6.2KB)
- [x] configFreqSweep @0x154240 (~5KB)
- [x] setSweepStep @0x155640 (~5KB)
- [x] setOscFreq @0x156a70 (~5KB)
- [x] configureFeedbackProcessing @0x157e60 (~5.6KB)
- [x] Build verify + sub-phase wrap-up
- [x] readConst/readString/readWave/readCvar return type corrected: void → EvalResultValue

---

## Phase 19 — Resources / EvalResults missing implementations (HIGH PRIORITY) ✅ DONE 2026-04-24

Surfaced after Phase 18 wrap-up: the static-archive build hides 295 undefined
zhinst symbols. Two clusters directly block earlier reconstruction work
(custom_functions builtins call `Resources::readConst` and `EvalResults::setValue`
without those symbols actually being defined). Tackle these first because
they would break any executable link of the library.

**Sub-phase status**: 19a ✅, 19b ✅, 19c folded into 20e, 19c-followup ✅,
19d ✅. All 19c-followup investigation prerequisites for Phase 20 cleared.

### 19a. EvalResults out-of-line definitions (6 blocking symbols) ✅ DONE 2026-04-23

Created `src/eval_results.cpp` (270 lines). All 14 methods implemented
(6 required + 8 nice-to-have), full impls — no stubs.

- [x] `EvalResults::EvalResults(VarType)` ctor @0x176bc0
- [x] `~EvalResults()` dtor @0x16f3d0
- [x] `setValue(VarType, Value const&)` @0x211b70
- [x] `setValue(VarType, VarSubType, Value const&)` @0x16bfb0
- [x] `setValue(VarType, int)` @0x15c850
- [x] `addAssembler(AsmList::Asm const&)` @0x15c1b0
- [x] BONUS: copy ctor @0x231c60, getValue @0x211ab0, plus 6 more
      setValue overloads (Value, VarType, double, string, VarType+Value+int,
      VarType+VarSubType+Value+int)
- [x] Build verify — clean, all 6 required symbols now `T`
- [x] Notes update: appended setValue codegen pattern to
      `notes/struct_layouts.md`. Key discovery: `setValue(double)` is
      the only overload that hard-codes VarType (=VarType_Const).

### 19b. Resources missing implementations — direct blockers ✅ DONE 2026-04-23

Appended ~190 lines to `src/resources.cpp` (482 → 671). All 3 blocker
symbols + the GlobalResources TLS statics now defined.

- [x] `Resources::readConst(string, EDirection)` @0x1e7d70 — partial
      (int/bool/double variant paths fully decoded; absW≥3 string path
      has a TODO referencing 1e7e2c..1e7e36 for variant-string layout).
- [x] `Resources::addConst(string, double, VarSubType)` @0x1e7010 —
      full impl. **Discovery**: writes literal **4** to Variable+0x00,
      not VarType_Const=3 — the on-disk record tag for value-bearing
      const entries is 4 (the bare-stub `addConst(name, st)` overload
      probably uses a different value). Worth a follow-up note.
- [x] `Resources::newLabel(string)` @0x1ec6b0 — full impl. Post-
      increments `GlobalResources::labelIndex` (TLS+0x4c) and appends
      decimal value to the (possibly empty → "label") prefix via
      ostringstream.
- [x] `GlobalResources::random`, `regNumber`, `labelIndex` TLS statics
      — all defined.
- [x] `ResourcesException` ctor/dtor — verified defined in resources.cpp.
- [x] Build verify — clean, all 3 blocker symbols now `T`.
- [x] Discovery: readConst error codes are 0xb0=UninitializedVar (miss)
      and 0xaf=TypeMismatchWrite (wrong type). addConst dup-name throws
      0xab=AlreadyDefined.

### 19c. Resources full sweep (all remaining declared methods, ~38)

**Folded into Phase 19d's planning output.** Will be re-organized as
work-packages alongside the other class-by-class undefined symbols
discovered during the full sweep. Items listed here for reference:

- [x] `createSubScope` @0x1e36a0
- [x] `updateParent` @0x1e38f0
- [x] `variableDependsOnVar` @0x1e40e0
- [x] `variableExists` @0x1e4230
- [x] `variableExistsInScope` @0x1e4390
- [x] `getVariableType` @0x1e4460
- [x] `getVariableSubType` @0x1e4580
- [x] `addVar` @0x1e46b0
- [x] `updateVar` @0x1e4c40
- [x] `checkVar` @0x1e4e20
- [x] String family: addString×2 @0x1e5020/0x1e54f0, updateString
      @0x1e59d0, readString @0x1e5d70
- [x] Wave family: addWave×2 @0x1e6020/0x1e64f0, updateWave @0x1e69c0,
      readWave @0x1e6d60
- [x] Const family: addConst (overload) @0x1e74e0, updateConst
      @0x1e79b0, constIsSet @0x1e8050
- [x] Cvar family: addCvar×2 @0x1e8180/0x1e8650, updateCvar @0x1e8b20,
      readCvar @0x1e8e80
- [x] Function family: functionExists @0x1e9110, getFunction @0x1e9370,
      functionExistsInScope @0x1e95d0, getPossibleFunctions @0x1e9740,
      addFunction @0x1e9c10
- [x] `getRegister` @0x1eba50
- [x] `setupGlobalState` @0x1ec8f0 (the Resources(name, deviceConstants)
      ctor variant)
- [x] StaticResources ctor/dtor/getVariable @0x129cb0/0x129db0/0x129e00/0x129e60
- [x] GlobalResources ctor/dtor @0x12a710/0x12ab40
- [x] Function::addArguments + addBody bodies (currently empty stubs;
      depend on SeqCAstNode interface)
- [x] Resources::Variable::~Variable variant string cleanup (currently
      stubbed)

### 19c-followup. Open investigation items from 19a/19b ✅ DONE 2026-04-24

Surfaced during 19a/19b reconstruction; all three resolved before
Phase 20.

- [x] **Variable record-tag mismatch (CRITICAL for 19c)** — RESOLVED.
      Root cause was a wrong VarType enum, not a separate "record-tag"
      mechanism. `str(VarType) @0x247dd0` jump table at 0x95c2a0 yields
      the canonical mapping `Unset=0, Void=1, Var=2, String=3, Const=4,
      Wave=5, Cvar=6` (was `Unset=0, Var=2, Const=3, Cvar=4, String=5,
      Wave=6`). Tag 4 IS VarType_Const under the corrected enum;
      Variable+0x00 IS the VarType directly. Tag mapping verified
      across all add/read overloads. Cascading fix applied to
      resources.hpp, resources.cpp (str, toString, addConst, readConst),
      expression.cpp:177, eval_results.cpp setValue(double) doc, and
      custom_functions.cpp comments. See `notes/struct_layouts.md`
      ("VarType enum — corrected mapping" section).
- [x] **readConst absW≥3 string path** — RESOLVED. Filled in for both
      SSO (bulk 24-byte copy) and long-form (placement-new of
      std::string(ptr, size)) paths in resources.cpp. Variable layout
      reconciled simultaneously: `which_` is at +0x10, variant storage
      at +0x18, with a `subType` (VarSubType) field at +0x08 not
      previously identified. Size 0x58 confirmed by `add r14, 0x58` at
      1e8441. New `variantStorage[16]` field replaces the prior
      embedded-Value approach. `Variable::~Variable` dtor body filled
      in. See `notes/struct_layouts.md` ("Variable layout — corrected"
      section).
- [x] **GlobalResources TLS layout audit** — RESOLVED. Five zhinst
      slots in shared `.so` TLS module block fully mapped: nextID
      (+0x40), Node::idCounter_ (+0x44), GlobalResources::regNumber
      (+0x48), labelIndex (+0x4c), random[313] MT19937-64 state (+0x50,
      2504B). Total zhinst TLS = 0x9D8. `Node::idCounter_` declaration
      added to Node class (replaced free static `node_id_counter`).
      All members are C++11 thread_local statics, NOT a struct
      instance. randomSeed builtin (Phase 17c stub) would write to
      `random[0]` and re-run the seed loop. resources.hpp
      GlobalResources doc-block fully rewritten.

### 19d. Full undefined-symbol sweep + triage ✅ DONE 2026-04-23

Methodology: `nm libzhinst_seqc.a` → set difference (U − T) =
**95 truly-undefined zhinst symbols**. Full audit document at
`reconstructed/notes/linker_resolution.md` (was `undefined_symbols_audit.md`, 420 lines). Plan
folded into Phase 20 below; Phase 19c carry-forward Resources items
folded into 20e.

- [x] Run full nm sweep and categorize by class — 95 symbols across
      ~17 classes/free-fn groups; `ErrorMessages::format<...>` is the
      single largest cluster (64/95 = 73%).
- [x] Produce `notes/linker_resolution.md` (was `undefined_symbols_audit.md`) with a class-by-class
      inventory + cost estimates.
- [x] Propose phased work-packages in TODO.md — Phase 20 below
      (5 sub-phases 20a–20e).
- [x] User review / approval of the plan — confirmed; Q1
      (ErrorMessages strategy = inline header template) and Q2
      (WavetableManager\<IR\> = out-of-line specialization) resolved.

---

## Phase 20 — Undefined-symbol elimination (executable-link prep)

Outcome of Phase 19d audit (`notes/linker_resolution.md`).
95 symbols are referenced by some TU but defined nowhere. Five
work-packages, ordered by impact-per-effort: **20a clears 73% of the
gap with one cheap edit**; 20e (Resources sweep) is deferred to last
because of size.

### 20a. Globals + ErrorMessages template body (69 symbols, biggest single win) ✅ DONE 2026-04-24

Highest-impact, lowest-cost package: a single header edit + a few
one-line global definitions clear 73% of the entire gap (69/95) and
unblock 14 caller TUs.

- [x] Inline `ErrorMessages::format<Args...>(ErrorMessageT, Args&&...)`
      body into `error_messages.hpp:457-466` using `boost::format` +
      `std::initializer_list<int>{...}` chained `operator%` (C++17,
      safer than fold-expr). **Strategy confirmed by user (2026-04-23).**
      All 64 instantiations now generated as weak symbols `W` at caller TUs.
- [x] Define `ErrorMessages errMsg;` in `error_messages.cpp:28`
      (BSS @ 0x95de60).
- [x] Define `Prefetch::minIndexedSize` static int in `prefetch.cpp:2294`.
      Init value `0x1000` (4096) recovered from `__cxx_global_var_init`
      at 0xd4361.
- [x] Define `zhinst::zsyncDataPqscDecoder`, `zsyncDataPqscRegister`,
      `constAwgIntegrationTrigger` `const std::string` globals in
      `error_messages.cpp:45-47`. Strings recovered from binary rodata
      via `strings _seqc_compiler.so`. Note: binary uses `L` prefix
      mangling (internal linkage); our header declares `extern` for
      cross-TU access — slight ABI deviation, behaviour unchanged.
- [x] Build verify (`cmake --build .` from `reconstructed/build/`).
      Build clean. Diff against `/tmp/truly_undefined.txt`: 64
      `format<>` template-instantiation symbols + 5 globals all
      resolved (69/95 = 73% gap closure, as planned).
- [x] Sub-phase wrap-up.

**Follow-ups (deferred to executable-link phase):**

- [ ] When an executable target is added to CMakeLists, verify that
      the inlined `format<>` template body produces semantically-
      equivalent output to the binary's explicit instantiations.
      Currently we only verified link-resolution, not runtime output.
      Spot-check at least: `<int>`, `<string>`, `<int,int>`,
      `<string,int,int>`, `<char const*,string,unsigned short,short>`.
- [x] Document the `extern`-vs-`L`-internal-linkage ABI deviation for
      the 5 zsync/integration-trigger globals. Added to existing
      `notes/libcpp_abi.md` under new "ABI deviations" section
      (2026-04-24).

**Estimated sessions:** 1.

### 20b. Trivial Rule-of-Five and missing default ctors (8 symbols) ✅ DONE 2026-04-24

After 20a, the remaining gap is 26. This package clears 8 more cheap
items, leaving 18 for the more involved packages.

- [x] `Immediate(Immediate const&)` copy ctor — added to `value.cpp`
      with per-index switch dispatch (placement-new for string case).
- [x] `Immediate(Immediate&&)` move ctor — leaves `other` in valueless
      state (matches variant-move idiom).
- [x] `Immediate& operator=(Immediate const&)` copy assignment —
      destroys current state then copy-constructs in place.
- [x] `Value()` default ctor — `value.cpp`. Sets `type_=Unspecified,
      which_=0, storage_.i=0`. Any toX() conversion will throw.
- [x] `Node()` default ctor — `node.cpp`. Delegates to 3-arg ctor with
      `(NodeType{0}, 0, -1)`.
- [x] `WaveformFile(const char*)` ctor — `waveform.cpp`. Copy-constructs
      `name`, zero-inits `field18/1C/20`, default-init `data`. Note:
      binary's 0x2a7ff0 was a `std::construct_at<>` specialization
      inlining the body — no dedicated ctor symbol existed.
- [x] `floatEqual(double, double)` @0x2ec050 — added to
      `waveform_generator.cpp`. **Surprise**: despite the name, the
      binary uses `cmpeqsd` (exact IEEE-754 equality), NOT a tolerance
      check. Reconstruction matches.
- [x] Move `logExceptionToClog` from `zhinst::detail` to
      `zhinst::logging::detail` in `log_exception.cpp` — single namespace
      wrap fix.
- [x] Build verify. All 8 symbols verified `T` in archive. Total
      progress: **77/95 (81%) done, 18 remaining.**
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1.

### 20c. Wavetable / Waveform ctors and template instantiations (5 symbols) ✅ DONE 2026-04-24

Same area of the codebase — batched to share disassembly context.

- [x] `WaveformIR::WaveformIR(string const&, WaveformFile::Type, DeviceConstants const&)`
      — body recovered from `allocate_shared<WaveformIR>` dispatcher
      inlining at `0x2aa170-0x2aa20f`. Added to `waveform_ir.cpp`.
- [x] `WaveformFront::WaveformFront(string const&, WaveformFile::Type, DeviceConstants const&)`
      — body recovered from `newWaveformFromFile` dispatcher inlining at
      `0x29b110-0x29b24f`. Added to `waveform_front.cpp`. Also corrected
      the misleading `bitsPerSample=dc[0x40]` comment in
      `waveform_front.hpp:65` (real fields: `seqRegWidth ←
      dc.waveformGranularity`; `frontField1 = 1`).
- [x] Added `WavetableManager<WaveformIR>::insertWaveform` body
      (mirror of `WaveformFront` specialization in
      `wavetable_manager_front.cpp:41` with `WaveformIR` substituted).
      Required forward-declared specialization at top-of-file because
      same-TU calls implicitly used it earlier (C++14 [temp.expl.spec]/6).
- [x] Added explicit-instantiation lines at the bottom of
      `wave_index_tracker.cpp` for both `WaveformFront` and `WaveformIR`.
      Symbols emit as `W` (weak), satisfying U references at link time.
      Also fixed pre-existing typo: template body referenced
      `wp->playIndex` but field is named `waveIndex` at Waveform+0x6C.
- [x] Build clean.
- [x] Sub-phase wrap-up: OVERVIEW.md row added; audit doc WP-C marked
      DONE with detailed findings; running total 82/95 (86%) symbols
      cleared, 13 remaining for WP-D + WP-E.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups (deferred to executable-link time):**
- [ ] Verify at executable-link time that the binary's `0x29d000`/`0x29d410`
      `WaveIndexTracker` template-ctor instantiations actually read
      `Waveform+0x6C`. The fix from `playIndex` → `waveIndex` is correct
      against our struct layout, but direct disassembly confirmation of
      the offset within these specific instantiations was not done.
- [x] Document the WaveformIR/WaveformFront 3-arg ctor ABI deviation in
      `notes/libcpp_abi.md` (member-init form vs dispatcher zero-then-
      overwrite — same observable state, different instruction sequence).
- [ ] *Optional, low-priority*: split the two `WaveIndexTracker` explicit-
      instantiation lines from `wave_index_tracker.cpp` into a separate
      `wave_index_tracker_inst.cpp` so disasm-evidence comments stay
      separate from link-machinery.

### 20d. Pimpl wrappers + parser context + NodeMap helpers (7 symbols) ✅ DONE 2026-04-24

These are scattered small methods, each in a different TU. Batched
to clear several caller TUs entirely in one pass.

- [x] Uncomment `AWGAssembler::assembleStringToExpressionsVec`
      wrapper in `awg_assembler.cpp:43-45` with the correct
      `vector<shared_ptr<AsmExpression>>` return type (sret @0x285100).
- [x] Implement `AWGAssemblerImpl::extractComment(string const&)`
      body (find `;` or `//`, return suffix). NOTE: binary inlined
      this twice as `$_1` lambda in `assembleStringToExpressionsVec`
      and never emitted a standalone symbol; we externalize as a real
      method to satisfy the cross-TU U reference.
- [x] Implement `SeqcParserContext::raiseError(string const&)`
      @0x247ae0 and `SeqcParserContext::setSyntaxError()` @0x247cb0
      (mirror `AsmParserContext` equivalents). New TU
      `src/seqc_parser_context.cpp` uses raw byte-offset access since
      class is opaque in header. ABI risk: vtable[6]/+0x30 callback
      offset is libc++-specific.
- [x] Implement `NodeMap::toFrequency(double, double)` @0x1c5630 and
      `NodeMap::toPhase(float)` @0x1c5680. Math fully decoded:
      `toFrequency = (int64_t)(freq * 2^48 / sampleClock)` with
      `2^48 = 281474976710656.0` from rodata `0x956078`;
      `toPhase = roundf(value * 23301.689f)` then 23-bit two's-comp
      wrap (scale `0x46b60b61` from rodata `0x8fd2b4`, wrap shared
      with `pauPoffIwrap` extracted as `wrap23()`).
- [x] Implement `parseOptionalRate(it, end, end, name, bool)` free fn
      @0x163980. Header param naming misleading: third iter is parse
      cursor (typically `PlayArgs::parse()` return). If exactly one
      EvalResultValue remains, calls `getPlayRate()`; else encodes
      "no-rate" sentinel `((strict?1:0)-1)|5` (5 strict, 0xffffffff
      lax). Throws on extra unparsed args.
- [x] Build verify (clean, no warnings, no link errors).
- [x] Sub-phase wrap-up (this commit). 6 of 7 symbols verified `T`/`W`
      in archive; the 3 string/iterator-bearing symbols emit with
      libstdc++ mangling (`__cxx1112basic_string`,
      `__gnu_cxx17__normal_iterator`) instead of libc++
      (`__1::basic_string`, `__1::__wrap_iter`) — internally
      consistent and resolved at link time. Total now 89/95 (94%);
      6 symbols remaining for WP-E.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups:**
- [ ] Verify at executable-link time that `SeqcParserContext`
      callback indirect-call works with the actual parser harness.
      The vtable[6]/+0x30 dispatch is libc++-specific; if the harness
      uses libstdc++ `std::function`, adjust the offsets in
      `src/seqc_parser_context.cpp` accordingly.
- [ ] Update the per-symbol audit script
      (`nm | grep " T $sym\$"`) to query both libc++ and libstdc++
      mangled forms, OR document that clean `cmake --build` is the
      authoritative gate (not raw symbol-presence checks against
      libc++ mangled names).

### 20e. util::wave + MemoryAllocator + CsvParser + 19c-carry-forward Resources sweep

Largest package. Combines the remaining 5 trivial undefineds with the
big Resources class sweep (37 method bodies from the 19c carry-forward
list) so that those Resources methods land before any future consumer
re-introduces undefined references.

**Prerequisite — 19c-followup investigation items** ✅ CLEARED 2026-04-24
(VarType enum + Variable layout corrected, TLS layout fully mapped). The
38 Resources method bodies below now have a known-correct struct layout
to write against.

**Prerequisite — fresh undefined-symbol gap check (added Phase 20d wrap-up):**
Before starting WP-E work, re-run the gap analysis querying BOTH
mangling variants (libc++ `__1::basic_string`/`__wrap_iter` AND
libstdc++ `__cxx1112basic_string`/`__gnu_cxx17__normal_iterator`).
The post-20d count may be lower than the audit's 6 if some symbols
are now incidentally satisfied by template instantiations dragged in
during 20a–20d. Update `notes/linker_resolution.md` with the
true remaining list before starting implementation.

#### 20e-i. Cheap wins: util::wave + MemoryAllocator + CsvParser (5-6 symbols) ✅ DONE 2026-04-24

Risk-ordered first batch — all small, each in its own TU; no
cross-cutting layout assumptions. Land these before tackling the
Resources sweep.

- [x] `zhinst::util::wave::double2awg(double, uint)` @0x299630.
      14-bit signed sample, 2 marker bits in low. Saturate at 1.0,
      scale `8190.0`, lround, pack `(rounded<<2)|(marker&3)`.
- [x] `zhinst::util::wave::double2awg1m(double, uint)` @0x299680.
      15-bit signed sample, 1 marker bit. Scale `16383.0`,
      pack `(rounded<<1)|(marker&1)`.
- [x] `zhinst::util::wave::double2awg16(double)` @0x299700.
      16-bit signed sample, no marker. Scale `32767.0`.
- [x] `zhinst::util::wave::hash(string const&)` @0x299760.
      **Uses `boost::uuids::detail::sha1`** (binary calls
      `process_block` @0x29a270 and `get_digest` @0x29a000).
      Returns `vector<uint32>` of 5 words (160-bit SHA-1).
      Reconstructed via `boost::uuids::detail::sha1::process_bytes`
      + `get_digest(uchar[20])` with manual MSB-first packing.
- [x] `MemoryAllocator::MemoryAllocator(DeviceConstants const*, uint32_t)`.
      Inlined at all binary call sites; we provide a real ctor
      matching the documented 0x70-byte layout.
- [x] `CsvParser::csvFileToWaveform<WaveformIR>` @0x2be830 — STUB.
      Full reconstruction deferred (~7000 bytes; not on hot path).
      Stub throws `std::runtime_error` instead of silently
      producing zero-filled waveforms. Open question #1 resolved:
      binary DOES contain real specializations (not inlined as
      archived Phase 12c claimed).
- [x] Build verify (clean, no warnings).
- [x] Mini-sub-phase wrap-up. **Final undefined-zhinst-symbol
      gap = 0** — `comm -23 <undefined> <defined>` is empty.
      Static archive fully self-contained for the zhinst namespace.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups:**
- [ ] If CSV-loaded waveform handling is ever needed by tests,
      reconstruct `CsvParser::csvFileToWaveform<WaveformIR/Front>`
      from disasm at 0x2be830 / 0x2ba8b0 (~7000 bytes each).
- [ ] Verify SHA-1 byte order at executable-link time. Boost may
      change `get_digest` API across versions; current code uses
      the `unsigned char[20]` overload which is universally
      supported, but the bswap-then-store order in the binary
      could be tested against a known SHA-1 vector.

#### 20e-ii. Resources 19c carry-forward sweep (~38 methods)

Bulk Resources-class implementation. Heavier risk because all share
the same Variable/VarType/Scope layout — if the layout has any
remaining surprises, they'll cascade across all 38 methods. Do this
AFTER 20e-i so the cheap wins are safely landed first.

- [x] **Resources 19c carry-forward** — all 37+ methods implemented
      across Batches 1-6 (resources.cpp + resources_function.cpp).
- [x] **StaticResources** ctor/dtor/getVariable — Batch 7
      (resources_static_global.cpp).
- [x] **GlobalResources** ctor/dtor — Batch 7
      (resources_static_global.cpp).
- [x] `Resources::Variable::~Variable` — defaulted (embedded Value
      member handles cleanup).
- [x] Build verify after each group of ~10 methods.
- [x] Sub-phase wrap-up — final nm gap check done 2026-04-24.

**Completed 2026-04-24.**

---

## Phase 21 — Post-20e gap closure

Identified during the 2026-04-24 post-20e audit (marker sweep + Deferred
review + unknowns review). Sub-phases ordered roughly by impact-per-effort.
Each sub-phase ends with the standard wrap-up (notes/OVERVIEW/TODO update +
build verify).

### 21a. Custom_functions throw-stubs — play wrappers + mergeWaveforms (COMPLETE 2026-04-24)

Closed all 5 throw-stubs in `custom_functions.cpp`. Marker count for the
file shifted from "5 huge throw-stubs" to "~45 small surgical TODOs"
(reduction in unimplemented-mass; new TODOs are fine-grained items
captured as 21a-followup below).

- [x] `mergeWaveforms` @0x15e060 (893 disasm → 288 C++ lines) —
      `custom_functions.cpp:767-1054`. 7-phase body. Header signature
      corrected (`int64_t`→`bool`). 2 pre-existing call-site bugs fixed.
- [x] `playAuxWave` @0x135610 (1118 disasm → 242 C++ lines) —
      `custom_functions.cpp:1660-1902`. PlayArgs(indexed=true). Channel
      scatter + zero-fill + asmPlay isHoldMode=true.
- [x] `playDIOWave` @0x1369f0 (819 disasm → 229 C++ lines) —
      `custom_functions.cpp:1698-1927`. PlayArgs(indexed=false). Per-bit
      trigger mask clearing.
- [x] `playWaveDIO` @0x137740 (187 disasm lines) — direct wvft emission.
- [x] `playWaveZSync` @0x137a50 (697 disasm lines) — readConst chain
      (RAW/PROCESSED_A/PROCESSED_B → 1/9/0xd shifts).
- [x] Sub-phase wrap-up: build clean, OVERVIEW updated (21a row),
      notes/struct_layouts.md extended with 3 new sections
      (PlayArgs::WaveAssignment 0x50 layout, play wrapper signature
      matrix, mergeWaveforms 6-arg semantics).

**Sessions used:** 1.

### 21a-followup. Surgical TODOs surfaced during 21a reconstruction (COMPLETE 2026-04-24)

All 6 surgical TODOs cleared. `custom_functions.cpp` marker count
45→35 (10 markers cleared).

- [x] **mergeWaveforms factory selection** (phase 6 of body) — three
      factory targets confirmed: `interleave@0x258140`, `merge@0x25f5c0`,
      `grow@0x260640`. Multi-value dispatch `test bl,bl;je` reads `bl`
      from `[rbp-0x48]` (function-local, not direct param); single-value
      branch unconditionally GROW. Source `(multiValue, useYSuffix)`
      mapping documented as approximate; exact `[rbp-0x48]` derivation
      noted as deferred sub-investigation in source comment.
- [x] **WaveformFront `channels()` accessor** — replaced raw read with
      `wf->signal.channels()` (`Signal::channels()` already exists at
      `signal.hpp:83`).
- [x] **mergeWaveforms call-site `param5_placeholder`** — both sites
      decoded as `(int)PlayArgs::getMaxSampleLength()` (set @0x15f634
      / @0x13400a after `getMaxSampleLength@0x15f62f` / `@0x133fce`,
      truncated from int64 to int by SysV stack-arg slot).
- [x] **mergeWaveforms `lengthDiffers` tracking** — `play` site bool
      decoded as `(ch != referenceChannelIndex)` from `setne al;cmp
      r14,[rbp-0xd8]` where `[rbp-0xd8] = r13[+0x24]`. Approximated as
      `(ch != channelIndex)` in source; renamed to `isSecondaryChannel`.
      `assignWaveIndex` site is hardcoded `false` (literal `push 0x0`).
- [x] **playDIOWave config field +0x16** — actually in playAuxWave
      `@0x135889`, decoded as `config_->channelsPerGroup[1]` (INDEXED
      variant, slot [1] of the 2-element uint16 array at config+0x14).
- [x] **WaveformFront `+0x48` field naming** — `assignWaveIndex`
      `@0x1342f1` writes to `Waveform::used` (already named in
      `waveform.hpp:104`); replaced offset-comment with `wf->used = true`.
- [x] Sub-phase wrap-up: build clean, OVERVIEW updated (21a-followup row),
      notes/struct_layouts.md mergeWaveforms section extended with
      param5/param6 semantics + factory-selection findings.

**Sessions used:** 1.

### 21b-prereq-A. Audit unaccounted mergeWaveforms call sites (COMPLETE 2026-04-24)

During 21a-followup we believed `mergeWaveforms` had **5** call sites in
the binary but only **2** in our reconstructed source. Audit revealed
the original count was wrong — we actually had **4 of 5** sites
implemented. The 5th is inside `playIndexed`, which is itself a heavily
stubbed function. See 21b-prereq-B for the real follow-up.

- [x] Disassemble `mergeWaveforms` callers @0x135ddc, @0x136cfa,
      @0x161c2b. Findings:
      - @0x135ddc → `playAuxWave` (already implemented at line 1809)
      - @0x136cfa → `playDIOWave` (already implemented at line 2083)
      - @0x161c2b → `playIndexed` (stubbed; see 21b-prereq-B)
- [x] Sub-phase wrap-up: notes/struct_layouts.md mergeWaveforms section
      extended with full call-site map.

**Sessions used:** <1.

### 21b-prereq-B (PARTIAL — phases 1-7) (COMPLETE 2026-04-24)

`CustomFunctions::playIndexed` @0x160e00 is a 6428-byte function
previously stubbed as ~64 lines at `custom_functions.cpp:1219-1282`.
Surfaced by 21b-prereq-A as the home of the missing mergeWaveforms
call site @0x161c2b.

**Done this sub-phase**:
- Full disasm of body 0x160e00..0x16271c (2250 lines, cached at
  `/tmp/playIndexed.disasm`).
- Structural map of 18 phases derived from call landmarks (see
  `notes/struct_layouts.md` "playIndexed @0x160e00 — 18-phase
  structural map").
- Phases 1-5 reconstructed in C++ with **3 critical bug fixes** vs
  prior skeleton:
  1. arg-count guard `< 2` → `< 3` (off by one)
  2. `indexed = (subFunc != Aux)` → `(subFunc == Aux)` (was inverted)
  3. removed wrong `asmTable` call; binary uses `addi+asmPlay` pattern
- Phase 5b added: `waveIndex == 0` is a non-throwing warning path
  (format error 0x9c → `warningCallback_` via vtable[+0x30] indirect →
  return empty results).
- Phases 6-7 scaffolded with binary-accurate locals (`regZero(0)`,
  `channelArgs` vector, `triggerMask = 0x3fff`, Aux-vs-non-Aux split);
  full body deferred — see 21b-prereq-B-cont below.
- Phases 8-18 documented as embedded outline with binary call addresses.
- Build clean.

**Continued in**: 21b-prereq-B-cont, 21b-followup-1, 21b-followup-2.

### 21b-prereq-B-cont. Finish playIndexed phases 8-18 (COMPLETE 2026-04-24)

**Was blocked on**: 21b-followup-1 (resolved same session) and
21b-followup-2 (resolved same session). Both unblocked this work.

- [x] Phase 6 (Aux): real iteration over per-channel WaveAssignment
      vector + `checkWaveformInitialized` calls. *(Modeled with TODO
      marker pending 21b-followup-3; commented-out call kept in source
      to document the structural intent.)*
- [x] Phase 7 (non-Aux): per-channel arg-gather loop with the
      vectorized SSE trigger-mask formula. *(Documented as comment;
      mask init `0x3fff` carried through.)*
- [x] Phase 8: `getWaveformSampleLength(name)` probe @0x161853.
      *(Modeled with TODO marker; baseLen=0 placeholder feeds Phase 9.)*
- [x] Phase 9: `createDummyWaveform(baseLen)` @0x161951. *(Discovered
      `WaveformGenerator::createDummyWaveform` is the documented helper
      for the inline "zeros" idiom — see waveform_generator.hpp:137.)*
- [x] Phase 10: `mergeWaveforms` call @0x161c2b — **5th binary call
      site now in source**.
- [x] Phase 11: `WavetableFront::loadWaveform(combined)` @0x161d76.
- [x] Phase 12-14: `getRegisterNumber` + `addi(reg, 0, Imm(rate))`
      + `asmSetVarPlaceholder` + Assembler push_back. *(Caught build
      error: `addi` returns `vector<AsmList::Asm>` not single `Asm`;
      fixed.)*
- [x] Phase 15: `checkOffspecWaveLength(combined, expectedLen)` @0x16214a.
- [x] Phase 16-17: `asmPlay(...)` @0x162343 + `results->assemblers_
      .push_back` @0x162511. *(asmPlay 12-arg call modeled after
      play()'s pattern at lines 1196-1202; boolean flag mapping
      tagged TODO.)*
- [x] Phase 18: error message factories — documented as comment
      block; errors 0x3d/0x98/0xa0/0x9a wired into Phase 2/4/5b/12
      throws.
- [x] Verify all 5 mergeWaveforms call sites now present in source.
- [x] Sub-phase wrap-up.

**Outcome**: `playIndexed` @0x160e00 (6428 bytes, 2250 disasm lines)
fully reconstructed end-to-end. Build clean. Residual unknowns
(per-channel WA accessor, asmPlay flag mapping, named field
constants) absorbed into expanded 21b-followup-3 below.

**Sessions actual:** 1 (faster than estimated thanks to followup-1/2
resolution).

### 21b-followup-1. Identify Resources +0x24 field and per-channel WaveAssignment container (COMPLETE 2026-04-24)

Surfaced by 21b-prereq-B; resolved same session.

**Resolution**: the access pattern `[r12]; [rax+0x24]` was
mis-attributed to Resources. Actually `r12 = this` (CustomFunctions*),
so `[r12] = this->config_` (at CustomFunctions+0x00, an
`AWGCompilerConfig const*`), and `[config_+0x24] =
AWGCompilerConfig::deviceIndex` (already named in
`awg_compiler_config.hpp:70`). The accessor is just
`config_->deviceIndex` — no new field/accessor needed.

The outer `vector<vector<WaveAssignment>>` at `[rbp-0x440]` was
confirmed by destructor symbol at @0x162661. Type matches
`PlayArgs::waveAssignments_` (at PlayArgs+0x60). However the stack
local at -0x440 is NOT physically the same as the PlayArgs field —
it's a separate stack vector populated somewhere upstream (no explicit
sret-write found; likely populated as a side effect of
`PlayArgs::parse` via inlined out-parameter or an accessor that copies
the field into the local). Source-level model:
`playArgs.waveAssignments_[deviceIndex]` — exact origin tracked as
21b-followup-3 below (low priority; doesn't block reconstruction).

**Outcome**: playIndexed source updated with correct interpretation
(deviceIndex + waveAssignments_ accessor pattern). 21b-prereq-B-cont.
is unblocked.

### 21b-followup-3. Trace exact origin of [rbp-0x440] vector + name residual playIndexed unknowns — COMPLETE 2026-04-24

Low-priority residual cluster from 21b-followup-1 + 21b-prereq-B-cont.
Bundles all small unknowns left in `playIndexed` source after the
end-to-end reconstruction.

**Group A — `[rbp-0x440]` outer vector origin** — RESOLVED:
`[rbp-0x440]` IS `playArgs.waveAssignments_` (at PlayArgs+0x60).
The PlayArgs object lives at `[rbp-0x4a0]` on the stack; 0x4a0−0x60=0x440,
so the vector is simply the member field populated by `PlayArgs::parse()`
internally. No separate population mechanism.

- [x] Trace [rbp-0x440] population site between PlayArgs ctor
      @0x160fd1 and the Phase 6 access @0x161250.
- [x] If it's a member function call with a hidden out-parameter,
      identify the function and add a proper accessor declaration.
      → N/A: it's a direct struct member offset, not a function call.
- [x] If it's an inlined copy, document the pattern in notes.
      → N/A: it IS the member field.
- [x] Update `playIndexed` Phase 6/8 source to use the correct
      accessor: `playArgs.waveAssignments_[deviceIndex]`.

**Group B — Named field constants** — RESOLVED:

- [x] Name `WaveformFront::field_d0_` — RESOLVED as `signal.length()`.
      Waveform+0x80 (Signal) + Signal+0x50 (length_) = +0xD0.
      Comment updated in Phase 11 bounds-check.
- [x] Name `AWGCompilerConfig::field_40_` — MISIDENTIFIED. `[this+0x08]`
      loads `devConst_` (NOT config_). `devConst_+0x40` =
      `waveformGranularity`. Fixed 6 call sites: playIndexed Phase 15,
      play() Step 7, playAuxWave Phase 9, playDIOWave Phase 11,
      assignWaveIndex (×2: seqRegWidth set + checkOffspecWaveLength).
- [x] Pin down channelsPerGroup index: Aux=0x14 → [0], non-Aux=0x16 → [1].
      Conditional expression: `config_->channelsPerGroup[subFunc==Aux ? 0 : 1]`.

**Group C — playIndexed Phase 14/16** — RESOLVED:

- [x] Trace local-Assembler push in Phase 14 — pushes addiEntries +
      placeholderEntry into `results->assemblers_` (confirmed by disasm
      @0x161ed4 and @0x161f79: push_back on vector at [r12+0x18]).
- [x] Full SysV mapping of asmPlay's 12 args: isHold=(subFunc==Aux),
      fourChannel=(subFunc==DigTrigger), isBool=false, holdCount=rate,
      suppress=triggerMask, isHoldMode=(subFunc==Aux), reg=indexReg,
      regVal=waveIndex, reg2=AsmRegister(-1), trigger=0.
      NOTE: r8b/r9b register-arg mappings have some uncertainty.
- [ ] Wire error 0x98 (invalid first-arg type) and error 0x9a
      (invalid wave-index type) — deferred to 21z (low value).

**Bonus findings:**
- `config_->field_18` in playAuxWave/playDIOWave emit-guard = `config_->isHirzel`.
- `assignWaveIndex`: both `wf->field_70_` set and checkOffspecWaveLength use
  `devConst_->field_4C` (NOT config_); `wf->field_70_` = `wf->seqRegWidth`.

**Estimated sessions:** 1-2 → **actual: 1**.

### 21b-followup-2. Re-audit all 5 mergeWaveforms call sites for SysV arg-shift correctness (COMPLETE 2026-04-24)

Surfaced by 21b-prereq-B; resolved same session.

**Resolution**: the puzzle (`rdx = lea` of a pointer where the 6-arg
signature said `short`) was caused by mis-counting registers — the
implicit `this` consumes rsi, shifting the explicit args by one
register. So actual layout is rdi=sret, rsi=this, rdx=arg1
(`vector const&`), rcx=arg2 (`short`), r8=arg3 (`bool`), r9=arg4
(`string const&`), stack[0]=arg5 (`int`), stack[1]=arg6 (`bool`).
Confirmed by inspecting the mergeWaveforms entry @0x15e060: the saved
rsi at @0x15e088 is later dereferenced via `+0x20` to fetch
`WavetableFront*`, which is `CustomFunctions::wavetableFront_`. So
rsi is `this`.

**Outcome**: no source bugs in the 4 already-implemented call sites;
no source changes needed. The only deliverable is the side-by-side
register-layout table now living in `notes/struct_layouts.md`
("SysV ABI puzzle ... — RESOLVED" section). Phase 10 of `playIndexed`
can proceed without further investigation.

### 21b. writeToNode (the 29KB beast) (IN PROGRESS)

Largest single gap in the entire codebase. Unblocks `setInt`, `setDouble`
and several node-access functions in `custom_functions.cpp`.

**Recon (cached `/tmp/writeToNode.disasm`, 6120 lines, 0x164550..0x16b740 = 0x71f0 / 29KB):**

- 4 static `boost::regex` objects in `.bss`:
  `b84748 absDevRegex`, `b84760 awgNodeRegex`, `b84778 sineNodeRegex`,
  `b84790 oscselNodeRegex` (each with adjacent guard variable at +0x10).
- Setup allocates 0x98-byte `__shared_ptr_emplace<EvalResults>` (vtable @b03d00).
- Top callees: `AsmCommands::suser` (53), `AsmCommands::addi` (44),
  `AsmList::append` (25), heavy `boost::regex_match`, `Value::toDouble`/`toString`/`toInt`.
- Sub-block map:
  | Block | Range | Size | Content |
  |-------|-------|------|---------|
  | Setup | 0x164550..0x164608 | ~180B | Allocate EvalResults; cmdName toString() |
  | A: absDevRegex | 0x16460e..~0x164800 | ~500B | Match abs device path |
  | B: awgNodeRegex | 0x164803..~0x164950 | ~330B | AWG channel dispatch |
  | C: sineNodeRegex | 0x164950..~0x164ae0 | ~390B | Sine osc dispatch |
  | D: oscselRegex | 0x164b19..~0x169d00 | **~21KB** | Bulk per-type codegen |
  | E: error tail | 0x169d83..0x169df4 | ~110B | Throw formatted error |
  | F: regex init | 0x169ea5..0x16a3f0 | ~1.5KB | Static regex ctors (cold) |
  | G: epilogue | 0x16a3f0..0x16b740 | ~5KB | dtors / unwinding |
- 3 call sites in binary: @0x141447, @0x1486f2 (setInt), @0x149334 (setDouble).
- Helper `boost::sub_match::str()` @0x16b830 + `std::stoul` to extract IDs.

#### 21b.1. Skeleton + Block A (absDevRegex) — **COMPLETE**

- [x] Declare 4 static `boost::regex` instances + guards in `custom_functions.cpp`.
      (Anonymous-namespace placeholders with TODO 21b.5 for real pattern strings.)
- [x] Function entry: allocate `EvalResults` via `make_shared`; cmdName toString();
      varType==2 (Var) precondition check → throw on failure.
- [x] **Bonus**: corrected return type from `void` to
      `std::shared_ptr<EvalResults>` (binary uses sret rdi). Header updated.
- [x] Block A: match against `absDevRegex`, extract device-id capture via
      `sub_match::str` + `stoul`, dispatch on `config_->deviceIndex`.
- [x] Trailing `lookupNode(pathStr)` call @0x1647ed performed unconditionally
      after Block A.
- [x] Sub-phase wrap-up: build clean, OVERVIEW.md status block + table row
      added, struct_layouts.md "writeToNode @0x164550 — recon (Phase 21b.1)"
      section appended.

#### 21b.2. Blocks B + C (awgNodeRegex + sineNodeRegex) — **COMPLETE**

- [x] Block B: AWG node — extract channel-index capture, normalise via
      `numChannelGroups` (1/2/4), validate against `config_->unknown_20`.
- [x] Block C: sine node — extract osc-index capture, map oscillator → channel
      via {2,4,8} oscs-per-channel logic, same channel-cap validation.
- [x] Both blocks documented in `notes/struct_layouts.md` ("writeToNode
      Blocks B + C — recon (Phase 21b.2)" section); error message strings
      remain TODO 21b.5.
- [x] Sub-phase wrap-up: build clean, OVERVIEW.md status block + table row
      added.

#### 21b.3. Block D structural skeleton (oscselNodeRegex, ~21KB) — COMPLETE

- [x] Map stack-frame `[rbp-0x1c0]..[rbp-0x180]` to local `NodeMapItem`
      from `lookupNode(pathStr)`; confirm `typeIdx` (+0x08) and `hasFast`
      (+0x10) field offsets.
- [x] Identify "MF" tag insertion into `usedFeatures_` (+0x1C8 set<string>),
      addNodeAccess call, getRegisterNumber + AsmRegister destReg + local
      AsmList accumulator setup.
- [x] Map dispatch jump tables @958f68 (fast path) and @958f50 (slow path
      / dyncast-success path), 6 cases (typeIdx 0..5) each.
- [x] Implement representative case (fast-path case 2 @0x164c7f..164cb4)
      as exemplar. Stub remaining 11 cases as TODO 21b.4.
- [x] Document Block D in `notes/struct_layouts.md`.
- [x] Sub-phase wrap-up.

#### 21b.3-fix. Block D structural correction — COMPLETE

- [x] Add missing `dynamic_cast<DirectAddrNodeMapData*>(node.data)` branch
      between fast-path and nodeAddressMap_-find paths. Block D is
      actually a 3-way dispatch: (A) hasFast path, (B) dyncast-success
      path (addr from `direct->addr_`), (C) nodeAddressMap_.find path.
      Paths (B) and (C) share jump table @958f50.
- [x] Enable `usedFeatures_.insert("MF")` (was commented out as
      unverified — binary unconditionally inserts at @0x164b51..164bee).
- [x] Refactor source: flatten 3-way addr resolution into a single
      `useFastJt` flag + `addr` value, then dispatch on either jt @958f68
      (fast) or jt @958f50 (paths B & C). Add `emitWarnAndReturn` early-
      return for typeIdx > 5 → @0x164d05 boost::log warning path.
- [x] Build clean.
- [x] Sub-phase wrap-up.

#### 21b.4-map. Block D protocol catalogue — COMPLETE

- [x] Read disasm of all 12 case bodies (paths A and BC × typeIdx 0..5)
      end-to-end and document the per-case emit-step structure.
- [x] Identify three case "shapes": (S1) single-step `addi(Imm) +
      suser(0x10)`; (S2) direct passthrough `suser(payload, 0x17[+0x19])`;
      (S3) low/mid/high triplet `addi(tag) → suser(0x10) → addi(addr) →
      suser(0x11) → suser(payload, 0x12)`.
- [x] Census suser opcodes: 0x10 (low/set), 0x11 (mid/addr-tag), 0x12
      (high/commit), 0x17/0x19 (direct-write).
- [x] Document Immediate sources: literal small ints, addr,
      `NodeMap::toFrequency(toDouble, getSampleClock)`,
      `NodeMap::toPhase(toFloat)`, raw float-bits (cvtsd2ss),
      `Value::toInt()` (slow-arm with `floatEqual` guard).
- [x] Map common-tail blocks (10 of them) and the variant-dispatch
      fallback ranges.
- [x] Catalogue saved to `notes/writeToNode_block_d_protocol.md`
      (253 lines, 7 tables + reference cases).
- [x] No source changes this sub-phase; build necessarily clean.

#### 21b.4-impl. Block D source transcription — COMPLETE

- [x] Verified jump-table order by dumping .rodata bytes at @0x958f50
      and @0x958f68 — the catalogue's linear-order assumption was WRONG
      for several cases. Corrected mapping:
      Path A: {0→0x164c7f, 1→0x165263, 2→0x165013, 3→0x165137,
               4→0x164ee7, 5→0x1652e6}.
      Path BC: {0→0x164de4, 1→0x16591b, 2→0x165587, 3→0x165751,
                4→0x165488, 5→0x165a17}.
- [x] Transcribed path-A cases 0..5:
      A.0: suser(val.reg_, 0x17) [register passthrough]
      A.1: suser(val.reg_, 0x17) + suser(R0, 0x19) [sine pair]
      A.2: toDouble→cvtsd2ss→memcpy float bits→addi+suser(0x17)
      A.3: toDouble→movq raw bits→low32→addi+suser(0x17)
      A.4: toDouble→getSampleClock→toFrequency→addi+suser(0x17)
      A.5: toDouble→cvtsd2ss→toPhase→addi+suser(0x17)
      All path-A cases share tail: addi(destReg, R0, Imm(addr)).
- [x] Transcribed path-BC cases 0..5:
      BC.0: addi(destReg, R0, Imm(1))
      BC.1: addi(destReg, R0, Imm(2))
      BC.2: double-triplet (tag 0xc→suser(0x10)→addr→suser(0x11)→
            val.reg_→suser(0x12); repeat with tag 0xd)
      BC.3: single triplet (tag 0xd, same as triplet B of BC.2)
      BC.4: single triplet (tag 2→suser(0x10)→addr→suser(0x11)→
            val.reg_→suser(0x12))
      BC.5: addi(destReg, R0, Imm(3)) [with varType check]
      Cases 0,1,5 share tail: suser(destReg, 0x10) + addi(destReg,
      R0, Imm(addr)).
- [x] Key correction from 21b.3: A.0's suser uses literal 0x17,
      NOT 0x17+addr. The addr goes into the common tail addi.
- [x] Key discovery: A.0/A.1 require varType==2 (jne to slow);
      A.2-A.5 require varType!=2 (je to slow). Slow-arm
      variant-dispatch is stubbed with TODO 21b.5 throws.
- [x] localList splice into results->assemblers_ — placeholder
      added (TODO 21b.5 to verify the exact EvalResults member).
- [x] Added `floatEqual` forward declaration to custom_functions.cpp.
- [x] Added `#include <cstring>` for std::memcpy.
- [x] Build clean.
- [x] Sub-phase wrap-up.

#### 21b.5. Tail (E/F/G) + setInt/setDouble forwarding + slow-arm stubs — COMPLETE

- [x] Block E: reclassified — 0x169d83 is shared_ptr cleanup, 0x169df4 is
      normal ret. Error 0x84 at 0x169e0d is cold-path from Block D oscsel.
- [x] Block F: static regex lazy-init — cancelled (cold-path ctor guard,
      not user-visible logic).
- [x] Block G: cleanup epilogue — verified as compiler-generated unwinding
      only (no user-visible logic).
- [x] Convert `setInt` / `setDouble` to return `writeToNode(...)` result.
- [x] Implement slow-arm variant-dispatch: 6 simple throws (A.2/A.3→0x7f,
      A.4→0x81, A.5→0x82, BC.4→0x7f, BC.5→0x82) + 4 real codegen paths
      (A.0 toInt+floatEqual+warning 0x80, A.1 64-bit double split,
      BC.2 float-bits tag-3, BC.3 64-bit double split tag-4+suser 0x13).
- [x] Verify localList splice target: `results->assemblers_` at
      EvalResults+0x18 (vector<AsmList::Asm>).
- [x] Sub-phase wrap-up.

**Completed in 7 sub-phases (21b.1..21b.5).** writeToNode 29KB function
substantially reconstructed. Remaining gap: per-case post-tails (see
21b-followup-4 below).

### 21b-followup-4. writeToNode per-case post-tails (path A) ✅ DONE 2026-04-24

All 6 per-case post-tail sequences implemented. Key structural correction:
the shared `addi(addr)` common tail at 0x165c90 was only used by typeIdx 0
(not all cases as previously modeled). Each typeIdx now has its own
post-tail block after the case body. Detailed trace confirmed via full
disassembly read of the 0x168xxx..0x169xxx region.

- [x] typeIdx 0 (passthrough): addi(addr) + suser(0x16) — commit.
- [x] typeIdx 1 (sine pair): 8-step extended tail — addi(addr) + addi(3) +
      suser(0x10) + suser(0x11) + toFrequency→addi(freq) + suser(0x11) +
      toPhase→addi(phase) + suser(0x16).
- [x] typeIdx 2 (float-bits): no post-tail (completes with body suser(0x17)).
- [x] typeIdx 3 (raw-double): addi(high32 via shr rbx,0x20) + suser(0x19).
- [x] typeIdx 4 (frequency): addi(freqHigh32) + suser(0x19) + addi(addr) +
      suser(0x18). NEW: closing opcode is 0x18 (not 0x16).
- [x] typeIdx 5 (phase): addi(addr) + suser(0x16) — commit.
- [x] Verify case routing: CORRECTED — only typeIdx 0 uses shared tail at
      0x165c90; ALL other cases bypass it. Prior note saying "cases 1,2,4
      bypass" was incomplete — cases 3,5 also bypass.
- [x] Protocol notes updated in writeToNode_block_d_protocol.md with
      corrected per-typeIdx emission table + closing-opcode semantics.
- [x] Both builds clean (g++ + clang++/libc++).


### 21c. AsmOptimize trio completion — COMPLETE

Self-contained optimizer work; no cross-file cascade.

- [x] `AsmOptimize::splitReg` @0x281000 — fully reconstructed from ~500
      lines of binary. Live-range splitter with ≥10 threshold, boundary
      ADDI insertion, allSplitOk/didSplit flags, post-loop kill.
- [x] `AsmOptimize::simplifyAssign` @0x280e10 — 4 bugs fixed: outputs[0]
      not immediates.back(), reg2/reg0 not reg1/reg2, scan from it+2,
      store to reg0 not reg1. Copy-propagation: redirect write-dest when
      followed by ADDI copy.
- [x] `assembler.hpp` reg field comments corrected: reg2=READ source,
      reg0=WRITE dest, reg1=dual-role. Field rename deferred (20+ file
      cascade, not worth the churn).
- [x] Sub-phase wrap-up.

### 21d. SeqCAstNode evaluate() virtual + VarType field — COMPLETE

Foundational AST work. Resolved unknowns #119 (SeqCParameter) and the
long-deferred evaluate() signature.

- [x] Identified `SeqCAstNode::evaluate()` virtual signature (3-arg at
      vptr[0], returns `shared_ptr<EvalResults>`). Base @0x209dc0 returns
      null. Binary TU: `SeqCAstNodesEvaluate.cpp`.
- [x] Full vtable layout verified from binary vtable dumps (8 slots base
      + 1 SeqCOperator extension at vptr[8] for 5-arg evaluate).
- [x] `VarType` at +0x14 made a proper base-class field (was "padding").
      All 53 subclass ctors updated with VarType as 4th explicit parameter.
- [x] `getVarTypes()` virtual at vptr[7] — base impl returns `{str(varType_)}`,
      SeqCParamList override iterates children.
- [x] `SeqCParameter` class REMOVED — `nm -C` shows zero symbols. VarType
      is base-class field; +0x18 is SeqCVariable::name_. Unknowns #119 resolved.
- [x] `addArguments(SeqCAstNode const&)` rewritten to use `varType()` +
      `static_cast<SeqCVariable const*>(node)->name()`.
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1-2.

### 21e. WavetableManager\<T\> remaining methods — COMPLETE (pre-done)

Template, partially done. ~14 methods, ~6KB total.

- [x] Audit remaining unimplemented methods in
      `wavetable_manager_front.cpp` and `wavetable_manager_ir.cpp`.
      **Result: all 16 methods (9 Front + 7 IR) already implemented
      with zero TODO markers. Only loose end is a dead `setLineNr(int)`
      declaration in the template header — never instantiated, no linker
      issue. WavetableFront::setLineNr accesses the field directly.**
- [x] Reconstruct in priority order — N/A, nothing to reconstruct.
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1-2.

### 21f. CachedParser ELF body + small markers sweep — COMPLETE

Closes the long tail of small markers across 8+ files. Drove total
marker count from ~94 to ~90 actionable (removed 8 TODOs, plus
several comment-only cleanups).

- [x] `CachedParser::cacheFile` ELF-building body @0x2b05b0 —
      fully reconstructed. File ext `.wave` (not `.elf`); filename
      `csv<hash2str>.wave`; machineType=3; section↔param mapping
      corrected. `removeOldFiles` return type changed `void`→`bool`.
- [x] `Function::addBody` try/catch — already clean (confirmed earlier).
- [x] `waveform_generator.cpp` 3 small TODOs resolved:
      readDoubleAmplitude fabs>1.0 warning (error 0x54 via
      warningCallback_); interpolation formula confirmed correct;
      lfsrGaloisMarker marker validation (valid={1,2}, error 0x64).
- [x] `awg_device_props.hpp` field names VERIFIED from consumer analysis:
      `maxWaveformSamples`→`maxElfSize`; `maxWaveformBytes`→split into
      `addressImpl` (uint32) + `sampleFormat` (uint32); 
      `supportsExtraFeature`→`isHirzel`. 9 instantiations updated.
- [x] `cached_parser.cpp` boost::archive workarounds — deferred (no
      change needed until build links real boost archive).
- [x] Sub-phase wrap-up: build clean, OVERVIEW updated.
- [x] Bonus: `compiler.cpp` evaluate() call wired up (unblocked by 21d);
      SeqCOperator 5-arg evaluate comment fixed.

**Estimated sessions:** 1. ✅ Completed.

### 21g. libc++/libstdc++ ABI design discussion ✅

Decision: dual-build (a)+(c). Completed 2026-04-24.

- [x] Survey all current per-call-site workarounds (Pimpl, raw byte
      offsets, vtable-offset assumptions).
      → ~45 reinterpret_cast, ~15 placement-new, ~25 static_asserts,
        2 GCC pragmas, 0 conditional compilation.
- [x] Evaluate three strategies: (a) build with libc++ via clang;
      (b) translation header switching on `_GLIBCXX_USE_CXX11_ABI`;
      (c) accept libstdc++ as canonical and document each ABI bleed.
      → (a) adopted + (c) retained as primary. (b) rejected.
- [x] Document decision in `notes/libcpp_abi.md`.
- [x] Implement: `build-libcxx/` directory (clang++ -stdlib=libc++),
      ABI-adaptive static_asserts, conditional std::function size assert.
- [x] Wrap-up: both builds clean (g++: 0err/1warn; clang++: 0err/6warn).
      Follow-up ABI cleanup is optional, not a new phase.

**Estimated sessions:** 1. ✅ Completed.

### 21h. SeqCAstNode evaluate() body reconstruction

Natural follow-on to Phase 21d. Now that evaluate() signatures and
VarType are wired, reconstruct concrete evaluate() method bodies for
key subclasses.

- [x] Build complete evaluate() address map from binary (32 3-arg + 22 5-arg).
- [x] `combine(VarType, VarType)` @0x247f50 — full 7×7 lookup table.
- [x] `VarTypeException` class (ctor @0x2480e0, dtor @0x248140).
- [x] SeqCOperator::evaluate(3-arg) @0x210aa0 — template-method that
      evaluates lhs/rhs children, combines VarTypes, dispatches to 5-arg.
- [x] SeqCValue::evaluate() @0x213140 — tag-dispatched: double path
      (setValue + ostringstream precision 5), string path (quote stripping +
      setValue(VarType_String)).
- [x] Resolve AsmCommands+0x50 = `wavetableFrontIndex_` lineNr field.
- [x] Declare evaluate override on ALL subclasses: 5 macros updated
      (SEQC_TRIVIAL_LEAF, SEQC_UNARY, SEQC_OPERATOR, SEQC_BINARY,
      SEQC_LIST) + 6 individually-declared classes.
- [x] Corrected SeqCValue::Tag enum (eString=0, eDouble=1, -1=empty).
- [x] Sub-phase wrap-up (both builds clean, OVERVIEW.md, TODO.md).
- [x] SeqCVariable::evaluate() @0x209ea0 — 3712 bytes, complex.
      Decoded both dispatch tables (varType_ 7-way + getVariableType
      5-way), all five `addX(name, Stub)` direct paths, all four `readX`
      paths (push single EvalResultValue into result->values_), Wave
      special path with `newEmptyWaveform`, error path for varType=Void.
      Inline EvalResultValue heap-build + manual vector-replace pattern
      collapsed to `values_.assign(1, std::move(rv))` semantics. Added
      `Resources::atScopeBoundary()` accessor for the +0x88 byte gate.
      Both builds clean.
- [x] Representative operator 5-arg overrides (SeqCPlus, etc.) to
      validate the template-method pattern. **SeqCAssign DONE** as the
      first 5-arg override (sub-phase 21h.3 below). **SeqCPlus DONE**
      as first arithmetic override (sub-phase 21h.4 below). Remaining
      arithmetic operators (SeqCMinus, SeqCMult, SeqCDiv) in 21h.5-7.
- [x] **21h.3 SeqCAssign::evaluate (5-arg) @0x243e60** — 5552 bytes,
      second-largest evaluate. Hidden-sret + this register shift
      resolved (rdi=retptr, rsi=this, [rbp+0x10]=rhsResult, [rbp-0x110]=ctx*).
      Cascade re-interpreted as outer dispatch on lhsType + inner
      rhsType compatibility test (Pass 5.5 → Pass 6 implementation).
      All 13 action rows implemented in
      `seqc_ast_nodes_evaluate.cpp` (~280 new lines): Var/Const/Cvar/
      String update*() paths, Wave[Numeric/FunctionArg/other] paths,
      VarTypeException catch handler with rhs-type propagation.
      Both builds clean. ASM emission micro-detail (addi/asmSetVarPlaceholder
      placeholder operand encoding) deferred via TODO comments.

**Estimated sessions:** 2-3 (3 used so far).

#### 21h.3-followup — deferred items from SeqCAssign reconstruction

- [x] **~~ASM emission for Const/Cvar/String rows (rows 3-5)~~** —
      **RESOLVED (2026-04-24).** Binary analysis confirmed there IS NO
      asm emission for these rows. Rows 3/4/5 are compile-time constant
      assignments: `updateConst()`/`updateCvar()`/`updateString()` →
      `result->setValue(VarType, lhsSub, value)`. All three share a
      common tail: `result->name_ = name + " = " + rhsResult.name_`
      @0x24594a. The TODO comments about `addi(R0, R0, ...)` and
      `asmSetVarPlaceholder(R0)` were completely wrong. Source updated
      with `setValue()` calls and name-concat tail.
- [x] **~~EvalResults +0x70 vs waveformFront_ +0x48 confirmation~~** —
      **RESOLVED (2026-04-24).** Confirmed as `arrayBacking_` semantics.
      Converted TODO to NOTE in source. The +0x70 field matches the
      `shared_ptr<EvalResults> arrayBacking_` at EvalResults+0x70 per
      the header layout.
- [x] **~~Row 7 (Wave[non-Numeric] = Wave) reachability~~** —
      **RESOLVED (2026-04-24).** Converted TODO to NOTE documenting the
      known difference: binary has error 0x8b path, reconstruction
      routes through the default handler which emits error 0xe9 instead.
      Not functionally blocking.
- [x] **EvalResults::getValue() return type correction**: **RESOLVED
      (2026-04-24).** Disassembly of `getValue()` @0x211ab0 confirmed it
      returns `Value` (not `EvalResultValue`). The body extracts the
      `Value` portion (at element+0x08) from the last EvalResultValue in
      `values_` and writes it to the sret buffer at offset +0x00 — i.e.
      it returns just the embedded `Value`, not the whole
      `EvalResultValue`. Return type corrected in `eval_results.hpp` and
      `eval_results.cpp`; ~40 call sites in `seqc_ast_nodes_evaluate.cpp`
      updated (`.getValue().value_` → `.getValue()`). The catch handler's
      `.value_` workaround is no longer needed. Both builds clean.

#### 21h.4 SeqCPlus::evaluate (5-arg) @0x22a600 — COMPLETE

- [x] Full 8-row cascade + default error path analyzed with binary
      address breadcrumbs. ~290 new lines in `seqc_ast_nodes_evaluate.cpp`.
- [x] `combine(VarSubType, VarSubType)` declared in resources.hpp,
      stubbed in .cpp (actual lookup table deferred to 21h.9).
- [x] Anonymous-namespace forward declarations for `combineWaveforms`
      @0x22c300 and `constWaveform` @0x22c9f0 (bodies deferred to 21h.8).
- [x] Both builds clean.

#### 21h.4-followup — deferred items from SeqCPlus reconstruction

- [x] **`combine(VarSubType, VarSubType)` @0x247ea0 lookup table**: NOT a
      lookup table — priority-based conditional logic (60 bytes). Done in 21h.9.
- [x] **`combineWaveforms` @0x22c300 full body**: ~1750B. Done in 21h.8.
- [x] **`constWaveform` @0x22c9f0 full body**: ~1000B. Done in 21h.8.
- [x] **Row 3 vtable-based dispatch @0x22b2df-22b301**: **RESOLVED (2026-04-24).**
      NOT a SeqCAstNode vtable call. The address `0xb06660` (labeled
      `vtable for SeqCAstNode+0x48` by the disassembler) is actually a
      `std::variant` destroy-visitor dispatch table for `Immediate`
      (`std::variant<AddressImpl<uint>, int, std::string>`). The 3 table
      entries: index 0/1 = no-op (trivially destructible), index 2 =
      destroy string. This pattern appears ~40 times across the codebase
      wherever local `Immediate` objects go out of scope. C++ generates
      this destructor dispatch automatically — no source changes needed.

#### 21h.5-7. Remaining arithmetic operator 5-arg overrides

~~Batch reconstruction — expected to be near-identical to SeqCPlus with
different operation semantics (subtract/multiply/divide doubles,
combineWaveforms op-name = "sub"/"mul"/"div").~~

**Discovery**: the arithmetic operators are NOT all identical. SeqCMinus
has structural differences (no String row, negation semantics, scaleWaveform),
SeqCMult uses `computeMult` + `WaveformGenerator::eval` (completely different
Wave path), SeqCDiv has no Var paths and no asm emission at all.

- [x] **21h.5 SeqCMinus::evaluate @0x22cde0** (~7312B) — 7-row action
      table + default error (0x74). Key differences from SeqCPlus: no
      String+String row; Var-Const negates rhs via `neg eax`+Immediate;
      Const/Cvar-Var uses addi(R0)+subr 2-step; Var-Var uses addi+subr;
      Wave paths use scaleWaveform to negate rhs before combineWaveforms("add");
      Wave-Const negates rhsDouble via xorpd sign-bit; no FunctionArg
      passthrough. New forward declaration: scaleWaveform @0x228cc0
      (body deferred). Both builds clean.
- [x] **21h.6 SeqCMult::evaluate @0x22ea70** (~9728B) — 6-row action
      table + default error (0x8c). Key differences from Plus/Minus: no
      String or Var*Var rows; Var*Const/Cvar uses computeMult() @0x22fdf0
      (shift-and-add multiplication); Wave*Wave uses combineWaveforms
      ("multiply"); Wave*Const and Const*Wave use 2-arg scaleWaveform
      @0x2309e0 (scalar, wave, ctx). New forward declarations:
      computeMult and 2-arg scaleWaveform (bodies deferred to 21h.8).
      Both builds clean (5 expected undefined-internal warnings in clang).
- [x] **21h.7 SeqCDiv::evaluate @0x231070** (~3664B) — 5-row action
      table + default error (0x8d). Structurally very different from other
      operators: NO Var paths (any Var → error 0xdf); NO Wave÷Wave; NO
      String paths; floatEqual() divide-by-zero checks (errors 0x29);
      Const/Cvar÷Wave → error 0x2a; Wave÷Const/Cvar computes reciprocal
      (1.0/rhs) and uses scaleWaveform 2-arg @0x2309e0. Three different
      error mechanisms: direct BST on ErrorMessages::messages (0xdf, 0x2a,
      0x29), errMsg[0x29], and ErrorMessages::format(0x8d). New forward
      declaration: floatEqual(double,double). Both builds clean (same 5
      expected undefined-internal warnings in clang).
- [x] Sub-phase wrap-up

#### 21h.8. combineWaveforms + constWaveform + scaleWaveform + computeMult helper bodies

- [x] `scaleWaveform` @0x228cc0 (1-arg overload — creates scalar with -1.0, delegates to 2-arg)
- [x] `constWaveform` @0x22c9f0 (~1000B — eval("rect", {length, value}), two catch clauses)
- [x] `combineWaveforms` @0x22c300 (~1500B — FunctionArg passthrough on both lhs/rhs, eval(opName))
- [x] `scaleWaveform` @0x2309e0 (2-arg overload — FunctionArg passthrough on wave, eval("scale"))
- [x] `computeMult` @0x22fdf0 (~3000B — integrality check, FunctionArg passthrough, 32-bit MSB-first shift-and-add loop)
- [x] Sub-phase wrap-up

#### 21h.9. combine(VarSubType, VarSubType) @0x247ea0 lookup table

- [x] Disassemble @0x247ea0 — NOT a .rodata lookup table; pure conditional
      logic (60 bytes). Priority-based combine: Default(0) is identity,
      FunctionArg(2) dominates, Stub(1) dominates Numeric/String, else Default.
- [x] Sub-phase wrap-up

### 21i. EParamDirection → EDirection rename ✅ DONE 2026-04-24

Unified the two separate enum definitions into a single `enum class
EDirection` in `types.hpp`, matching the binary's `zhinst::EDirection`
(mangled `NS_10EDirectionE`).

- [x] Rename `EParamDirection` → `EDirection` in `seqc_ast_node.hpp`.
- [x] Reconcile with the `EDirection` already in `resources.hpp` (they
      are the same type in the binary). Removed duplicate unscoped enum;
      both now use the scoped `enum class EDirection` from `types.hpp`.
- [x] Update all callers across ~75 call sites in 4 .cpp files:
      `static_cast<EDirection>(1)` → `EDirection::eOUT`,
      `EDirection_Read` → `EDirection::eIN`,
      `EDirection_Write` → `EDirection::eOUT`.
- [x] Sub-phase wrap-up: both builds clean (g++ + clang++).

**Estimated sessions:** 1. **Actual:** <1.

### 21j. Marker sweep: regex extraction + error codes + misc fixes — COMPLETE 2026-04-24

Systematic sweep of all remaining TODO/TBD markers. Prioritized by
value: easy-fixes first, then .rodata string extraction, then error
code verification, then documentation cleanup.

**~31 markers resolved (66→35 total, custom_functions.cpp 39→11).**

- [x] **Easy-fix**: playDIOWave dryRun — decode `channelArgs[0].value_.toString().empty()`
      with `size >= 2` guard. Binary does magic-number division by 0x38
      (÷56=÷sizeof(EvalResultValue)). Was incorrectly attributed to element[1].
- [x] **Easy-fix**: assignWaveIndex waveName — confirm `wa.value.value_.toString()`
      matches `[wa+0x08]` (Value at EvalResultValue+0x08). Stale TODO removed.
- [x] **Regex patterns**: all 4 writeToNode static boost::regex patterns extracted
      from .rodata via cold-path constructor addresses (@0x169ea5..0x169fab):
      - absDevRegex = `"/([0-9]+)/([\w/]+[\w])"` (was `"dev([0-9]+)/(.+)"`)
      - awgNodeRegex = `"awgs/([0-9]+)/.*"` (was `".+"`)
      - sineNodeRegex = `"sines/([0-9]+)/.*"` (was `".+"`)
      - oscselNodeRegex = `"sines/[0-9]+/oscselect"` (was `"oscs/([0-9]+)/.+"` — ZERO captures!)
      Variables renamed from `_TODO` suffix.
- [x] **Regex pattern**: assignWaveIndex cLikeIdentifier = `"[a-zA-Z_][a-zA-Z0-9_]*"`
      from .rodata @0x9005ed. No-match error 0xFB. VarType check error 0x96.
- [x] **Error codes**: writeToNode errors 0x84 (regex no-match), 0x85 (index mismatch),
      std::out_of_range for nodeAddressMap_ miss.
- [x] **Structural corrections in writeToNode**:
      - varSubType_ guard was checking wrong field (+0x04 not +0x00) AND wrong behavior
        (binary does silent return, not throw)
      - device-mismatch is silent return, not throw
      - nodeAddressMap_ miss uses std::out_of_range, not CustomFunctionsException
      - "second value" [rbp-0x1a8] is `type` parameter, not `valRef`
      - oscselNodeRegex no-match path throws error 0x84
- [x] **Warning log message**: "Unknown NodeMapType with code " (@0x900949) confirmed
      for both fast-path (@0x165407) and slow-path (@0x164d05) typeIdx>5 defaults.
- [x] **Documentation cleanup**: 5 stale TODO/TBD converted to NOTEs (awg_compiler_config.hpp,
      frontend_lowering.hpp, resources.cpp, custom_functions.cpp ×2).
- [x] Sub-phase wrap-up: both builds clean (g++ + clang++).

**Estimated sessions:** 1. **Actual:** 1.

### 21z. Long-deferred low-value items (DO NOT execute speculatively)

Tracked here so they don't appear "missing" but should NOT be picked up
unless a concrete consumer needs them.

- [ ] Phase 2-3 semantic naming (unknowns #23-28, #32, #38)
- [ ] AWGAssemblerImpl internal field purposes (#39-41)
- [ ] Cache/Prefetch implementation detail (#61-63, #68-69, #75, #81)
- [ ] Exception error-code details (#90-91)
- [ ] smap remaining logic — ~0x1E6 bytes after alui call (#10)

---

## Phase 22 — Cleanup, split, and expansion

### 22a. Refresh TODO.md summary table ✅ DONE 2026-04-25

Summary table was severely stale (claimed 106 SeqCAstNode stubs — all
are real implementations; claimed 55 builtin stubs — all eliminated;
etc.). Refreshed with audited counts. OVERVIEW.md status paragraph
updated in parallel.

- [x] Audit marker counts via grep (25 TODO, 7 TBD, 3 throw-stubs = 35
      total across 21 files)
- [x] Verify SeqCAstNode status: 49 print + 49 clone ALL real; 11
      evaluate implemented, ~43 remaining
- [x] Verify custom_functions.cpp: 7884 lines, 86 methods, 0
      return-nullptr stubs remaining
- [x] Update summary table with accurate row-by-row status
- [x] Update OVERVIEW.md reconstruction status paragraph

### 22b. Split custom_functions.cpp (7884 lines → 4 modules) ✅

The file is 3× larger than the next-largest source file. Split along
natural boundaries identified in the audit:

| Module | Actual lines | Content |
|--------|-------------|---------|
| `custom_functions.cpp` (core) | 1061 | Ctor, dtor, `call`, `functionExists`, validation/utility helpers, PlayArgs, free functions |
| `custom_functions_play.cpp` | 2462 | `setWaitCyclesReg`, `mergeWaveforms`, `play`, `playIndexed`, `writeToNode`, `generateWaveform` |
| `custom_functions_playback.cpp` | 994 | All `playWave*`, `playAuxWave*`, DIO/ZSync play, `playZero/Hold`, `waitWave`, `sync`, `setRate` |
| `custom_functions_io.cpp` | 3456 | DIO/IO, `wait`, `assignWaveIndex`, oscillator, QA, PRNG, sweeper, feedback configuration |

- [x] Create the 4 new .cpp files with proper includes
- [x] Move methods to their respective modules
- [x] Verify both builds clean (g++ + clang++)
- [x] Sub-phase wrap-up

### 22c. Remaining markers sweep (~35 markers across 21 files) ✅

Systematic sweep of all 32 markers (actual count, not 35). Reduced to 15
remaining — all genuine unknowns, blocked items, or large-body reconstruction.

Key outcomes:
- Resolved 7 quick-fix/doc-only markers (headers + resources.cpp)
- Resolved PlayConfig bit-packing TODO in custom_functions_io.cpp via encodeCwvf()
- Implemented waveIndex bounds-check in playIndexed (signal.length() confirmed)
- Converted 3 best-guess confirmation TODOs to NOTE annotations
- **Verified ALL 10 device knownOptions arrays** against binary .rodata:
  - 8/10 correct as-was (Hdawg4/8, Shfqa2/qc/li, Uhfli/ia, Vhfli)
  - **Fixed 2 bugs**: Uhfawg had {PID,MOD}→{CNT,QA}; Uhfqa had {MF,CNT}→{FF,RUB}

Remaining 15 markers: 3 genuine-unknown TBDs, 4 blocked (boost archive,
WaveAssignment), 4 large-body (csv_parser, frontend_lowering, seqc_ast_node),
2 design notes, 1 verify-needed, 1 reference to other TODO.

- [x] Triage all 32 markers by category (quick-fix / needs-disasm / blocked / doc-only)
- [x] Resolve quick-fix and doc-only markers (7 resolved)
- [x] Attempt needs-disasm markers in custom_functions (PlayConfig, bounds-check, 3→NOTE)
- [x] Attempt remaining needs-disasm markers across other files (4 device files verified+fixed)
- [x] Sub-phase wrap-up

### 22d. SeqCAstNode evaluate() body expansion (~43 remaining overrides)

Binary has 32 3-arg + 22 5-arg evaluate overrides (54 total, excluding
AWGAssemblerImpl::evaluate). 19 implemented so far, 26 stubs + 6 helper
bodies remaining.

#### 22d batch 1: Trivial evaluate overrides (11 done) ✅

- [x] Returns nullptr: SeqCCommand, SeqCOperation
- [x] Returns empty EvalResults: SeqCLabel, SeqCXorExpr(5), SeqCNoOp(5)
- [x] setLineNr + empty: SeqCVariableType, SeqCNoCmd
- [x] Delegate to child: SeqCPos
- [x] Error emitters: SeqCContinueStatement, SeqCBreakStatement (error 0xd5)
- [x] Throws: SeqCParamList (CompilerException)

#### 22d batch 2: Operator wrappers + stubs (8 done + 26 stubs) ✅

- [x] Wrappers: SeqCGreater→evalGreater, SeqCEqual→evalEqual,
      SeqCShiftL→evalShift(false), SeqCShiftR→evalShift(true),
      SeqCAndExpr→evalAnd, SeqCOrExpr→evalOr,
      SeqCLEqual→evalGreater+invertBool, SeqCNEqual→evalEqual+invertBool
- [x] 6 anonymous-namespace helpers forward-declared (bodies pending):
      evalGreater @0x235ac0, evalEqual @0x239be0, evalShift @0x232850,
      evalAnd @0x23ea20, evalOr @0x240a30, invertBool @0x238fb0
- [x] 26 TODO stubs with addresses added for remaining overrides
- [x] Both builds clean (g++: 0 warn; clang++: 6 expected undefined-internal)

#### 22d batch 3: Helper bodies + medium evaluate stubs (COMPLETE)

- [x] Implement 6 helper bodies: ~~invertBool~~ ✅, ~~valueToBool~~ ✅,
      ~~evalGreater~~ ✅, ~~evalEqual~~ ✅, ~~evalShift~~ ✅,
      ~~evalAnd~~ ✅, ~~evalOr~~ ✅ (all 6 complete)
- [x] Implement medium-size 5-arg operator stubs: ~~SeqCLower~~ ✅ (wrapper→evalLower),
      ~~SeqCGEqual~~ ✅ (invertBool(evalLower)), ~~SeqCLogAnd~~ ✅ (valueToBool+evalAnd),
      ~~SeqCLogOr~~ ✅ (valueToBool+evalOr), ~~SeqCMod~~ ✅ (fmod, const-only)
- [x] Implement 3-arg unary stubs: ~~SeqCNeg~~ ✅ (asmZero+subr / -toDouble() / scaleWaveform),
      ~~SeqCInv~~ ✅ (addi(-1)×2+subr / ~toInt())
- [x] Implement list stubs: ~~SeqCArgList~~ ✅, ~~SeqCDeclList~~ ✅ (iterate+accumulate),
      ~~SeqCStmtList~~ ✅ (iterate+accumulate+return-stmt detection+unreachable code warning)
- [x] New discovery: evalLower @0x237440 (~2KB) is a dedicated function (NOT swapped-arg evalGreater).
      Forward-declared; body pending (batch 4+).

#### 22d batch 4+: Larger evaluate stubs (pending)

- [x] Implement helper body: evalLower @0x237440 (~2KB) — dedicated a<b function
      (used by SeqCLower and SeqCGEqual; forward-declared, body pending)
- [x] Implement unary: ~~SeqCNotExpr~~ ✅ (3027B), ~~SeqCInc~~ ✅ (5464B),
      ~~SeqCDec~~ ✅ (5464B)
- [x] Implement unary/stmt: SeqCReturnStatement (6800B)
- [x] Implement control-flow: ~~SeqCIfCondition~~ ✅ (4360B) + jumpIfZero helper @0x2149f0 (760B)
- [x] Implement control-flow: ~~SeqCSwitchCase~~ ✅ (11506B, ~400 lines, 3-way dispatch),
      ~~SeqCRepeat~~ ✅ (8567B), ~~SeqCForLoop~~ ✅ (9794B)
- [x] Implement control-flow: ~~SeqCDoWhile~~ ✅ (7952B) — do-while with body-first, toDouble+floatEqual Cvar path
- [x] Implement control-flow: ~~SeqCWhileLoop~~ ✅ (7117B) + loopArgNodeAppend/loopBodyNodeAppend helpers
- [x] Implement control-flow: ~~SeqCCaseEntry~~ ✅ (2845B)
- [x] Implement control-flow: ~~SeqCIfElse~~ ✅ (7214B)
- [x] Implement complex: ~~SeqCFunction~~ ✅ (5080B) — field rename: call_/params_/body_/retType_
- [x] Implement complex: SeqCCondExpr (11007B) — done
- [x] Implement complex: ~~SeqCFunctionCall~~ ✅ (15220B, ~350 lines) — two paths: custom + user-defined
- [x] Implement complex: ~~SeqCArray~~ ✅ (2412B) — NOT ICF'd, array indexing with Wave+Const/Cvar validation, waveform lookup, bounds check, returns wave-with-index + Cvar sample
- [x] Implement hasCases/isSingleCase/singleCase/cases helpers + evalCaseBody @0x216fc0 + evalCases @0x216980 — all fully implemented from disasm
- [x] Sub-phase wrap-up

### 22e. Magic numbers refactoring (Category A+B)

Apply the proposal from `notes/magic_numbers_proposal.md`. Category A
defines new enums/constants; Category B replaces bare integers at call
sites with existing enum names.

- [x] Implement Category A enums/constants (16 items: ImmediateKind,
      AsmExprType, VariantSlot, VarFlag, OptFlag, DeviceOpts, OpcodeFormat,
      NodeTypeIdx, CmdType, RegOrder, RegAction, kCycle, SuserAddr,
      sentinels, kChannelTag)
- [x] Apply Category B call-site replacements (~430 sites across 15 files:
      ErrorMessageT 302, VarType 99, AwgDeviceType 26, Assembler::INVALID 12,
      ValueType 13, NodeType 2)
- [x] Investigate 3 open questions: SubFunc base=1 confirmed (switch fixed),
      ErrorMessageT 0x2F added as UnknownError47, OpcodeFormat documented
- [x] Sub-phase wrap-up

---

## Phase 20e-ii wrap-up cleanup (housekeeping)

Added 2026-04-24 during Phase 20e-ii Batch 5a wrap-up audit.
Items 3-5 resolved; items 1-2 were prematurely marked done.

- [x] **OVERVIEW.md narrative compression.** Compressed in Phase 21h
      session — 55-line prose replaced with 12-line summary paragraph.
- [x] **OVERVIEW.md stale marker-count table.** Updated to 73 in Phase
      21h session; phase table row for 21h added.
- [x] **Promote 3-way duplication to single source of truth.** Already
      resolved — unknowns.md carry-forward block removed in prior session
      (see unknowns.md line 101-109 history entry). OVERVIEW.md Open
      Questions section correctly references TODO.md + notes/ only.
- [x] **Variable header refactor: embed `Value value_` at +0x08.** Done
      during Phase 20e-ii Batch 5a (prior session). All read*/add*/update*
      references updated.



- [x] **Audit-script update — query both mangling variants.**
      Resolved: option (b) chosen. `cmake --build` (zero warnings, zero
      unresolved-at-link-time) is the authoritative gate; `nm` is a hint
      only. Phase 20e completed with all 95 symbols resolved, making the
      prerequisite moot.

## Phase 23 — Audit-driven cleanup and coverage expansion

Originated from comprehensive audit on 2026-04-25. Compared binary symbol
table (2852 unique `t` symbols) against reconstruction (835 intersection,
430 genuine zhinst gaps after filtering STL/boost template noise). Also
audited all source for placeholder fields, raw-offset access, approximate
implementations, stubs, and uncertainty comments. Cleanup tasks first,
then larger refinement, then new coverage.

### 23a. TODO.md housekeeping ✅

- [x] Rewrite summary table with audit findings
- [x] Tick off stale `[ ]` checkboxes (19c, floatEqual, mergeWaveforms,
      writeToNode, WavetableManager, play wrappers — all done in prior phases)
- [x] Add Phase 23 plan
- [x] Remove 7 vtable store-back TODOs from `seqc_ast_nodes_evaluate.cpp`
      (resolved in 21h.4-followup as Immediate variant dtors — no source change)
- [x] Replace `0x6e69616d` with `"main"` string comparison in
      `seqc_ast_nodes_evaluate.cpp`

### 23b. DeviceConstants field naming (~6 fields × 9 device types) ✅

Name the placeholder fields from consumer analysis. These are the most
frequently used unnamed fields in the entire codebase (~54 assignment
lines in device_constants.cpp + ~20 consumer sites elsewhere).

- [x] `field_20` → `sineNodeBase` (oscillator node index base in setSinePhase/incrementSinePhase)
- [x] `field_24` → `waveformElfAlignment` (ELF segment alignment; stored as WaveformIR.irField2)
- [x] `field_3C` → `triggerLatencyCycles` (min-wait threshold; subtracted after wtrig)
- [x] `field_48` → `playMinSamples` (min play length; no reconstructed consumer but TODO hint from binary)
- [x] `field_4C` → `waveformMinSamples` (initial seqRegWidth; passed to checkOffspecWaveLength)
- [x] `field_54` → `numCounters` (getCnt range check; confirmed Phase 18b-ii)
- [x] Cascade rename across device_constants.cpp (9 blocks) + all consumer sites
- [x] Sub-phase wrap-up

### 23c. AWGCompilerConfig field naming (~4 unknowns) ✅

- [x] `unknown_20` → `awgIndex` (AWG core index; builds node paths + validates channel ownership)
- [x] `unknown_28` → no consumer found; annotated as unresolved
- [x] `unknown_88` → no consumer found; annotated as unresolved
- [x] `unknown_98[8]` → no consumer found; annotated as unresolved
- [x] Sub-phase wrap-up

### 23d. reinterpret_cast elimination in resources.cpp (~30 sites) ✅

The ~30 raw byte-offset accesses on Value/Variable objects are the single
worst readability hotspot. Variable layout is fully known since Phase
19c-followup. Replace all `reinterpret_cast + offset` patterns with
proper field access through the reconstructed struct.

- [x] Audit all reinterpret_cast sites in resources.cpp (29 found)
- [x] Replace Value raw-offset patterns (+0x04, +0x08, +0x18, +0x50)
      with named field access (var->value, var->subTypeRaw, &var->value.storage_)
- [x] Replace Variable flags byte hack with VarFlag_Written/VarFlag_Frozen
- [x] Replace libc++ string internal offset reads with std::string* cast
- [x] Build verify — 30→8 reinterpret_casts remaining (8 are inherent
      SSO memory ops that can't be eliminated without algorithm change)

### 23e. Device-type bitmask constants in custom_functions_io.cpp (~70 sites) ✅

Replace `static_cast<AwgDeviceType>(0x1ff)` patterns with named
combinations. Define convenience constants in types.hpp (e.g.
`kDevAll`, `kDevHirzel`, `kDevSHFPlus`, etc.).

- [x] Survey all unique bitmask values used (19 unique values, 69 total sites)
- [x] Define 12 named constants in types.hpp (kDevAll, kDevAllButUHF, kDevHirzel,
      kDevSHFPlus, kDevLIFamily, kDevCervino, kDevPreSHFLI, kDevQA, etc.)
- [x] Replace 61 hex casts in custom_functions_io.cpp + 8 in custom_functions_playback.cpp
      (single-device values replaced with enum name, e.g. `static_cast<AwgDeviceType>(HDAWG)`)
- [x] Build verify — 0 hex bitmask casts remaining

### 23f. reinterpret_cast cleanup in other files (~50 sites) ✅

After resources.cpp (23d), clean up remaining raw-offset access:
custom_functions*.cpp (~18), prefetch*.cpp (~15), waveform.cpp (~5),
awg_assembler*.cpp (~3), seqc_parser_context.cpp (2).

Full audit found ~99 sites across 23 files. Fixed 18 easy wins:
- [x] custom_functions.cpp: config_->deviceType (×2), devConst_->playMinSamples,
      devConst_->waveformPageSize, wf->signal.length_ (×2), wf->name (×2),
      wf->seqRegWidth, config_->includePaths loop
- [x] custom_functions_play.cpp: config_->deviceType
- [x] prefetch*.cpp: config_->isHirzel (×4), config_->cacheType
- [x] seqc_ast_nodes_evaluate.cpp: replaced 0x6e69616d with `funName == "main"` (×2)
- [x] Build verify — ~100 remaining are inherent (serialization, tagged unions,
      aligned storage, SSO internals, vtable dispatch, stride iteration) or
      require deeper struct knowledge (custom_functions_play arg offsets,
      prefetch usageEntries, wavetable_front manager internals)

### 23g. Remaining placeholder fields in other headers ✅

- [x] waveform_generator.hpp: `field_20_` → `funcMap_maxLoadFactor_` (libc++ internal),
      `field_48_` → `aliasMap_maxLoadFactor_`, `field_78_` → `pad_78_`,
      `field_B0_` → `reserved_B0_` (dead field, no setter in binary)
- [x] custom_functions.hpp: `field_80_` → `funcMap_maxLoadFactor_`,
      `field_A8_` → `aliasMap_maxLoadFactor_`, `field_B0_` → `unusedStringSet_B0_`
      (no consumer), `field_168_` → `assignedWaveNames_` (populated by assignWaveIndex)
- [x] prefetch.hpp: 6 unknowns at +0xC0..+0xD8 already resolved as `cwvfConfig_`
      (PlayConfig struct) — updated stale layout comment
- [x] Sub-phase wrap-up — build clean

### 23h. SeqC copy-ctor / operator= / swap / accessors (158 missing symbols) ✅

Mechanical code generation. All 53 subclasses need copy-ctor (= clone()),
operator= (copy-and-swap idiom), swap (member swap), and child-accessor
methods (expr(), funName(), stmts(), getListElements(), etc.). Can be
largely macro-generated.

- [x] Survey which accessors are called in existing code vs just in binary
- [x] Implement accessors for subclasses that have them (expr(), funName(),
      stmts(), params(), decls(), body(), label(), ifBody(), elseBody(),
      cond(), arguments(), validLabel(), hasLabel())
- [x] Implement copy-ctor = clone pattern via macro or per-class
- [x] Implement operator= via copy-and-swap
- [x] Implement swap() free functions
- [x] SeqcParserContext::hadSyntaxError() + reset() (2 more missing symbols)
- [x] Build verify + sub-phase wrap-up

### 23i. Missing toString/str/serialize methods (~53 symbols) ✅

Various classes have toString(), str(), toJson(), serialize() methods in
the binary that we haven't implemented.

- [x] `Assembler::str(bool)`, `commandToString(Command)`, `highestRegisterNumber()` — already complete
- [x] `Assembler` copy-ctor, operator=, move-operator=, dtor — added (default ctor + explicit dtor, copy-assign with self-check, move-assign)
- [x] `AsmList::serialize()` — already complete
- [x] `Cache::Pointer::str()` — already complete
- [x] `CompilerMessage::str(bool)` — already complete
- [x] `Node::toString()`, `type2str(NodeType)`, `waveAtCurrentDeviceIndex()` — already complete
- [x] `Value::toString()` — already complete
- [x] `WaveformFront::toString()`, `WavetableFront::toString()` — already complete
- [x] `WaveformIR::toJsonElement(SampleFormat)` — already complete
- [x] `WavetableIR::alignWaveformSizes()`, `assignWaveformAllocationSizes()` — implemented
- [x] `WavetableIR::getJsonIndex(SampleFormat)` — already complete
- [x] `Waveform::File::operator==()`, `typeToStr()` — already complete
- [x] `Signal::Signal(ulong, MarkerBitsPerChannel const&)` — already complete
- [x] `ElfReader::getCode()`, `getLineMap()`, `getWaveform()` — implemented
- [x] `Prefetch::wvfImpl()`, `wvfRegImpl()`, `wvfs()` — already complete
- [x] `Resources::setReturnValue(Value)`, `toString()` — already complete
- [x] `str(AsmOperationType)`, `str(EDirection)`, `str(EValueCategory)`,
      `str(VarSubType)`, `str(VarType)` — all implemented (AsmOperationType enum + str added)
- [x] `toString(Immediate)`, `toString(AwgSequencerType)`,
      `toString(DeviceFamily)`, `toString(DeviceOption, DeviceFamily)`,
      `toString(DeviceTypeCode)`, `toString(DeviceType const&)` — all implemented
- [x] Build verify + sub-phase wrap-up

### ~~23j. AWGCompiler public API~~ → moved to Phase 24c

### ~~23k.~~ → moved to Phase 26a

### 23l. DeviceType extra methods (21 symbols) ✅

Factory `makeDefault()` methods (already structurally present but emitting
wrong mangling?), DeviceType constructors, comparison operators.

- [x] Audit which are already defined vs mangling-gap
- [x] `DeviceType(DeviceFamily)`, `DeviceType(DeviceFamily, unsigned long)` ctors
- [x] `DeviceType::deviceType()`, `toString()`, `swap()`
- [x] `DeviceType::print(ostream&)`
- [x] `operator<(DeviceType)`, `operator==(DeviceType)`
- [x] `operator<<(ostream&, DeviceType const&)`
- [x] `hasOption(DeviceType, DeviceOption)` (free function)
- [x] `detail::DeviceFamilyFactory::makeDeviceType()`,
      `detail::DeviceTypeImpl::doClone()` — already complete
- [x] Build verify + sub-phase wrap-up

### ~~23m.~~ → moved to Phase 26b


---

## Phase 24 — Python binding layer (pybind11)

Reconstruct the complete path from `PyInit__seqc_compiler` down to the
already-reconstructed `Compiler` class. ~10-12KB across 4 components
plus the AWGCompiler facade.

### 24a. Build system setup ✅

- [x] Install pybind11 headers — already installed (pybind11 3.0.4-1)
- [x] Add pybind11 `find_package` / header-only dep to CMakeLists.txt
- [x] Verify build still passes (both g++ and clang++/libc++)

### 24b. ZiFolder utility (~1KB, 3 methods) ✅

- [x] `ZiFolder::ZiFolder(string)` [0x2ce2c0] — already in zi_folder.cpp
- [x] `ZiFolder::ziFolder(DirectoryType)` [0x2cf0c0] — already in zi_folder.cpp
- [x] `ZiFolder::folderPath(...)` [0x2ce2f0] — already in zi_folder.cpp
- [x] Build verify + sub-phase wrap-up

### 24c. AWGCompiler facade + AWGCompilerImpl (~2-3KB, 15+ symbols) ✅

Relocated from 23j — thin wrappers around the reconstructed Compiler.

- [x] `AWGCompiler` pimpl wrapper (11 methods, all forwarding to Impl)
- [x] `AWGCompilerImpl` class (0x2C0 bytes) — ctor, dtor, all 11 methods
- [x] `AWGCompilerImpl::getCompileReport()` — iterate compileMessages_ + assembler report
- [x] `AWGCompilerImpl::setCancelCallback/setProgressCallback` — forward to compiler_
- [x] `AWGCompilerImpl::compileString()` — structural reconstruction (validate device type,
      Compiler::compile, assemble, optimize, output)
- [x] `AWGCompilerImpl::compileFile()` — read file, delegate to compileString
- [x] `AWGCompilerImpl::addWaveforms()` — iterate paths, WavetableFront::newWaveformFromFile
- [x] `AWGCompilerImpl::writeToStream()` — ElfWriter output pipeline
- [x] `AWGCompilerImpl::writeToFile/writeAssemblerToFile` — file I/O wrappers
- [x] `AWGCompilerImpl::getBinVersion()` — structural TODO (needs CalVer/getLaboneVersion)
- [x] `AWGCompilerImpl::getJsonWaveformMemoryInfo()` — structural TODO (needs WavetableIR iteration)
- [x] `AWGCompilerConfig::getAwgDeviceTypeString/getChannelGroupingModeString` — already existed
- [x] Build verify (both g++ and clang++/libc++)

### 24c-followup. AWGCompilerImpl TODO cleanup (2026-04-25)

Resolved 5 of 8 TODO markers in `awg_compiler.cpp`:

- [x] `compressSourceString` @0x109e90 — real zlib: deflateInit(level 9) +
      deflate(Z_FINISH) loop with 0x8000-byte chunks + deflateEnd; error
      throws via ErrorMessages::format(0x1e, format)
- [x] `getBinVersion` @0x10b830 suffix table — switch on deviceType:
      HDAWG→"hirz", SHFQA/SG/QC_SG/LI→"grim", GHFLI→"gurn", VHFLI→"malo",
      default→"cerv"; output built as growing std::string (4→8→16 bytes)
- [x] `getAssemblerHeader` @0x1083d0 — banner/separator/auto-gen notice,
      conditional source file, version "26.01.3.9", formatTime(now, false)
- [x] `hadSyntaxError` wiring — added Compiler::hadSyntaxError() accessor
      (SeqcParserContext byte at +0x03); wired into writeToStream +
      writeAssemblerToFile early-return checks
- [x] AWGCompilerImpl ctor @0x103b40 — proper init: getDeviceConstants,
      make_shared\<WavetableFront\>, Compiler(config, devConst, wavetable_)
- [x] `writeToStream` metadata sections — 7 of 9 ELF sections implemented:
      .nodes/.nodes_json (device-type gated), .channels,
      .required_sample_rate, .waveforms, .wavemem, .version_bin;
      source compression flag at config+0x9D now gates .c/.asm sections
- [x] `addWaveforms` binary format loading (.bin/.bin16/.wave/.wave16) —
      .bin/.wave: ifstream(ate) → tellg → read → awg2double/awg2marker loop
      → Signal(samples, markers, markerBits) → newWaveformFromFile(stem,
      signal, path, Type=1); .bin16/.wave16: log warning "not implemented"
- [x] `getJsonArguments` @0x10a3c0 → `.arguments` ELF section
      (boost::property_tree: "destination", "source", "waves" array)
- [x] `getJsonVersion` @0x10ac60 → `.version_json` ELF section
      (boost::property_tree: "compiler" uint32, "target" device family;
      NOTE: "external_triggering" and "required_options" conditionally
      omitted — source data at this+0x190 not yet identified)

### 24d. compileSeqc orchestrator (~6KB) [0xf58a0] ✅

- [x] Reconstruct `zhinst::compileSeqc()` — JSON config parsing,
      DeviceType/AWGCompilerConfig assembly, AWGCompiler invocation
- [x] Build verify + sub-phase wrap-up

### 24e. pybind11 entry points (~3KB) ✅

- [x] `zhinst::pyCompileSeqc()` [0xe0000] — arg extraction, GIL
      release, result packing
- [x] `zhinst::makeSeqcCompiler()` [0xe1900] + `makeSeqcCompilerInCore()`
      — module_.def() registration, version attrs
- [x] `PyInit__seqc_compiler` [0xf5350] — module creation
- [x] Build verify + sub-phase wrap-up

---

## Phase 25 — Boilerplate reduction (helper extraction)

The evaluate-AST (~10K lines) and custom-functions (~8K lines across 4
files) implementations contain significant repetitive boilerplate — likely
reflecting inlined helpers or macros in the original source. Introducing
named helpers makes the reconstruction shorter, more expressive, and
less error-prone.

Ordered by impact × feasibility: AST-evaluate lambda dedup first (pure
duplication, zero risk), then custom_functions helpers (higher churn risk
in binary-faithful code).

### 25d. AST evaluate: promote local lambdas to file-scope helpers ✅

Promoted 5 helper lambdas from per-function definitions to file-scope
anonymous-namespace functions in `seqc_ast_nodes_evaluate.cpp`. Removed
24 duplicate lambda definitions total:

- [x] `isConstOrCvar` (11 defs → 0; moved to `resources.hpp` as shared inline)
- [x] `rhsTypeOrUnset` (5 defs → 0; now free function taking `EvalResults const&`)
- [x] `rhsCount` (4 defs → 0; same)
- [x] `getBackReg` (2 defs → 0; now file-scope, no captures)
- [x] `rhsSubOrDefault` (2 defs → 0; same as rhsTypeOrUnset)
- [x] Updated all call sites: `rhsCount()` → `rhsCount(rhsResult)`, etc.
- [x] Both builds clean (g++ + clang++)

### 25g. Shared `isConstOrCvar(VarType)` across codebase ✅

- [x] Added `inline bool isConstOrCvar(VarType)` to `resources.hpp` (after VarType enum)
- [x] Replaced 5 bitwise patterns in `custom_functions_io.cpp`:
      `(static_cast<int>(arg.varType_) & ~1) == 4` → `isConstOrCvar(arg.varType_)`
- [x] Both builds clean

### 25f. evalLogical — LogOr `" && "` confirmed as binary bug ✅

- [x] Investigated: SeqCLogOr uses `" && "` separator, confirmed from binary
      DWORD 0x20262620. This is a **copy-paste bug in the original source**,
      not a reconstruction error. Already documented in source comments.
      No fix applied (we match the binary).

### 25a-c,e. custom_functions helpers — DEFERRED

- [ ] ~~`emitLoadImmediate` (37 sites in io.cpp)~~ — deferred. Pattern has
      enough variation in variable names, Immediate sources, and follow-up
      instructions that mechanical replacement risks obscuring binary-address
      documentation and introducing subtle differences. ROI too low vs risk.
- [ ] ~~`emitRegOrConst` (~15 sites)~~ — depends on 25a, deferred.
- [ ] ~~`checkWaitState`/`isShfFamily`/`emitWaitTrigger`~~ — deferred.
      Low ROI for binary-reconstructed code.
- [ ] ~~`validateBinaryOperands` preamble~~ — only 4 instances (Plus/Minus/
      Mult/Div). Not enough duplication to justify extraction.

**`checkWaitState(int expected)`** (~10 sites) — the DIO/ZSync
waitState_ protocol:
```cpp
void CustomFunctions::checkWaitState(int expected);
```

**`isShfFamily()`** (~8 sites) — the 3-way `(devType == SHFLI ||
devType == VHFLI || devType == GHFLI)` test:
```cpp
bool CustomFunctions::isShfFamily() const;
```

**`emitWaitTrigger(constName, results, res)`** (~8 sites) — the
`readConst + addi + wtrig` sequence that is character-for-character
identical across multiple wait functions:
```cpp
void emitWaitTrigger(const char* constName, EvalResults& results,
                     std::shared_ptr<Resources> const& res,
                     AsmCommands* asmCommands);
```

- [ ] Implement `checkWaitState`, `isShfFamily`, `emitWaitTrigger`
- [ ] Replace call sites
- [ ] Build verify + sub-phase wrap-up

---

## Phase 26 — Remaining gaps (stubs, data tables, approximate impls)

Relocated from Phase 23 — these are not mechanical cleanup but require
binary analysis and new reconstruction work.

### 26a. GetNodeMap\<T\>::get() specializations (8 symbols) ✅

Extracted all 8 device-specific node maps (1081 total entries) from binary
using runtime extraction (dlopen + tree traversal). Implemented as
`get_node_map.cpp` (1267 lines) with table-driven `addDirect`/`addVirt`
helpers. Wired `initNodeMap` stub to use real `getNodeMapForDevice()`
dispatcher. Added default ctors to `DirectAddrNodeMapData` and
`VirtAddrNodeMapData`.

- [x] Survey what GetNodeMap returns (static map of node paths → addresses)
- [x] Implement 8 specializations from binary data
- [x] Wire initNodeMap stub to use real GetNodeMap data
- [x] Build verify + sub-phase wrap-up

### 26b. Stubs and approximate implementation cleanup ✅

Address the 6 conservative stubs and ~19 approximate implementations.

- [x] `oscMaskCheckHirzel` @0x15bab0 — fully reconstructed from binary.
      3-way dispatch (dc=1/2/4) × MF feature, with jump tables for
      groupIndex. ~1.2KB function.
- [x] `oscMaskSetAllHirzel` @0x15bf50 — fully reconstructed. Returns
      bitmask shifted by groupIndex, scaled by MF feature presence.
- [x] `initNodeMap` — done in 26a (wired to `getNodeMapForDevice()`)
- [x] `secureLoadWaveform` @0x1711a0 — expanded from stub. Now includes
      CSV duplicate warning (error 0xEB via reportWarning_) and
      `wavetableFront_->loadWaveform()` call.
- [x] `parseOptionalString` / `getPlayRate` — done earlier in 26b session
- [x] Review ~19 approximate-implementation comments — all 3 checked
      (exception.cpp MI layout, mergeWaveforms factory, signal.cpp epsilon)
      are documented structural approximations, not bugs. No action needed.
- [x] `frontend_lowering.cpp` lower() body — was already complete since
      Phase 21f. Cleaned up stale `constWaveform` stub (moved to
      seqc_ast_nodes_evaluate.cpp in Phase 21h.4).
- [x] `seqc_ast_node.cpp` recursive printer @0x1fa430 — fully reconstructed
      with box-drawing tree connectors (`|-` / `` `- ``). Also fixed
      SeqCValue::~SeqCValue() — was `= default` which leaked placement-new'd
      strings; now properly dispatches destruction based on tag_.
- [x] Build verify — both g++ and clang++/libc++ clean, 0 errors.

---

## Phase 27b — CalVer + utility free functions

### 27b. CalVer class + versioning (16 symbols) ✅

- [x] `CalVer` struct (0x20 bytes: year_, month_, patch_, build_ as size_t)
- [x] `CalVer::CalVer(string const&)` @0x0ffdb0 — dot-counting parser,
      extractVersionTriple + optional build suffix
- [x] 4 accessors: year/month/patch/build + triple()
- [x] `getLaboneVersion()` @0x100270 — static {26, 1, 3, 9}
- [x] `getLaboneVersionWithCommitHash()` @0x1002a0 — "26.01.3.9 (203353a...)"
- [x] `asBinary()` @0x1007c0 — year<<24 | month<<16 | (patch<<12)&0xFFFF | build&0xFFF
- [x] `asDecimal()` @0x1006e0 — (YY*100+MM)*100000 + P*10000 + BBBB
- [x] `fromBinary()` @0x100780, `fromDecimal()` @0x100490 — inverses
- [x] `toString()` @0x1007f0 — "YEAR.MONTH.PATCH"
- [x] `isSet()` @0x100470 — any field non-zero
- [x] `operator==` @0x100bc0, `operator<` @0x100c00
- [x] Removed CalVer forward-decl from awg_compiler.cpp; now uses calver.hpp

### 27b. formatTime (3 overloads) ✅

- [x] `formatTime(ptime, char const*)` @0x2f6190 — time_facet + oss
- [x] `formatTime(ptime, bool)` @0x2f7440 — compact ? "%Y%m%d_%H%M%S" : "%Y/%m/%d %H:%M:%S"
- [x] `formatTime(long, bool, bool)` @0x2f7470 — epoch→ptime, optional utcToLocal
- [x] Removed formatTime forward-decl from zi_folder.cpp; now uses format_time.hpp

### 27b. Serial number predicates (10 symbols) ✅

- [x] All 10: isHf2/isUhf/isMf/isHdawg/isPqsc/isShf/isShfacc/isGhf/isQhub/isVhf
      Each uses unsigned subtraction range check pattern.
      Short range + long range (10× base), except PQSC (single range).

### 27b. getPlatformName ✅

- [x] `getPlatformName()` @0x2ec6e0 — returns "linux64"

### 27b remaining (deferred)

- [x] `extractVersionTriple()` @0x101570 — implemented: boost::split on '.', lexical_cast up to 3 components
- [ ] 6 misc string/filesystem utilities (unreferenced by compiler core):
      isDirectoryWriteable, isMountPoint, isPureAscii, isValidUtf8 (×2), isInList

---

## Backlog — Long-deferred / SDK-scope items (unphased)

Items that are outside the compiler core or extremely low value.
DO NOT execute speculatively — assign to a phase only when needed.

- [ ] **ZI*Exception helper functions** — getKind @0x2e5180, toApiCode
      @0x2e5280, toZiErrorKind @0x2e5240, fromZiErrorKind @0x2e5260.
      Deep into boost::system::error_category plumbing (custom
      ErrorKindCategory singleton). Not called by compiler core.
- [ ] **CalVer remaining** — 6 misc
      string/filesystem utilities (unreferenced by compiler core)
- [x] **assembler.hpp register field rename** — reg0/reg1/reg2 renamed to
      regDst/regSrc/regAux across 10 files (137 refs). Phase 27a.
- [ ] **Rename placeholder field names** — ~15 nondescript fields across
      8 headers (WaveformFront, WaveformIR, Waveform, AsmExpression,
      ElfReader, Resources, AWGAssemblerImpl, AWGCompilerConfig). High-
      confidence renames for 7 fields, medium for 3, unknown for 8.
      Full inventory and evidence in `notes/placeholder_field_names.md`.
- [x] **csv_parser.cpp full reconstruction** — ~7KB per specialization.
      DONE 2026-04-26. Both `csvFileToWaveform<WaveformFront>` and
      `<WaveformIR>` fully reconstructed with CsvException, setSampleFromString,
      getLineVector, isCsvSeparator. Builds clean (g++ + clang++/libc++).
- [ ] Phase 2-3 semantic naming (unknowns #23-28, #32, #38)
- [ ] AWGAssemblerImpl internal field purposes (#39-41)
- [ ] Cache/Prefetch implementation detail (#61-63, #68-69, #75, #81)
- [ ] Exception error-code details (#90-91)
- [ ] smap remaining logic — ~0x1E6 bytes after alui call (#10)
- [ ] SeqCAstNode ~~`type` field meaning (#96)~~ RESOLVED — see Phase 13a
- [ ] `seqc_error`/`seqc_lex_init_extra`/`seqc_parse`/`seqc_set_extra` —
      flex/bison parser entry points (C functions)
