# Unknowns

Registry of open questions and resolutions for the zhinst SeqC compiler
reverse-engineering effort.

Original item numbers (1-116) are preserved for cross-reference from code
comments (`// see unknowns.md #97`). Numbers ≥117 are post-archive additions.
The full pre-2026-04-22 history is preserved in
[`archive/unknowns_full_1-116.md`](archive/unknowns_full_1-116.md).

---

## Open — tracked in TODO.md

All remaining open items are tracked as actionable work items in TODO.md
(Phase 31). Listed here for cross-reference only.

(none — all items resolved)

---

## Closed

| #   | Title | Resolution |
|-----|-------|-----------|
| 75  | cervino indexed nonsplit (2 stubs) | Resolved: play_cervino_indexed_nonsplit filled with prf(regH, regC, clampToCache(cacheSize/2)). Common indexed finalize split per-branch: Hirzel emits addr+goto play_common_prf; non-Hirzel emits addr+channels*totalSize+smap+addi+smap sequence. |
| 45  | assembleAsmList register ordering | Resolved: verified from disasm — order is regDst(+0x28) → regAux(+0x30) → regSrc(+0x20) → outputs(+0x38) → label(+0x50). Fixed cout message to match binary strings. |
| 55  | SeqcParserContext full layout (+0x08..+0x2F) | Resolved Phase 31d: full 0x38-byte layout decoded. +0x00..0x03 flag bytes, +0x04 lineNumber, +0x08 padding, +0x10 std::function errorCallback. All methods converted from raw-offset to typed member access. reset() and setErrorCallback() added. |
| 93  | Expression pushChild ownership model | Resolved Phase 31d: pushChild uses `shared_ptr<Expression>(raw)` with standard `default_delete` (vtable at 0xb03730, deleter at 0x129720). Full ownership transfer, no no-op deleter. Fixed expression.cpp. |
| 114 | CacheEntry serialize template body | Resolved Phase 31d: Binary serializes 5 fields (name_, filePath_, fileSize_, timestamp_, hash_), NOT 6. `valid_` is not serialized — confirmed from both text_iarchive @0x2b7700 and text_oarchive @0x2b8440 disassembly. Fixed cached_parser.hpp. |
| 10  | smap remaining logic | Resolved Phase 31a: ~0x1E6 bytes after alui are compiler-generated Immediate dtor cleanup + two `st()` calls + vector insert — all already implemented in asm_commands.cpp:617-634. |
| 18  | AssemblerInstr outputs population | Resolved 2026-04-26 grooming: populated in `asm_list.cpp:390` (single-output push_back) and `:523-528` (multi-output loop). Consumed by assembler.cpp, asm_optimize.cpp, awg_assembler_impl_pipeline.cpp. Blocker (expression evaluation) cleared by Phase 22d. |
| 23  | Node deviceIndex naming | Resolved 2026-04-26 grooming: field named `deviceIndex` at +0x40, JSON key `"deviceIndex"`, comment "index into wavesPerDev; -1 = none". |
| 24  | Node play field | Resolved 2026-04-26 grooming: named `play` at +0xA0, typed `vector<weak_ptr<Node>>`, JSON key `"play"`. |
| 25  | Node length field | Resolved 2026-04-26 grooming: named `length` at +0x90, `lengthReg` (AsmRegister) at +0x88. |
| 26  | toJson idMap | Resolved 2026-04-26 grooming: signature `toJson(const unordered_map<int,int>& idMap)` — idMap remaps nodeId to serializable indices. |
| 27  | Node serialize opcode==4 skip | Resolved Phase 31b: already correctly implemented. Pass 1 of AsmList::serialize() skips opcode==4 (NOP) entries from idMap. Pass 2 still emits them via assembler.str(true). |
| 28  | Node serialize #disableOpt handling | Resolved Phase 31b: condition was inverted — binary appends " #disableOpt" when `isWaveformCmd && opcode ∉ {3,4,5}`, not just opcode==5. Fixed in asm_list.cpp:201. |
| 32  | DeviceConstants anonymous enums | Resolved Phase 31a: only two anonymous enums exist in binary — SyncRegA=0x44 and SyncRegB=0x45 — both already documented in Register struct. |
| 38  | Signal marker bit distribution | Resolved Phase 31c: numEntries formula was `channels + (channels>1?1:0)`, binary uses `max(1, channels)`. Fixed in signal.cpp:69. |
| 39  | AWGAssemblerImpl +0x20 string | Resolved 2026-04-26 grooming: identified as `asmSource_` (std::string at +0x020). Full layout re-derived in Phase 4/9. |
| 40  | AWGAssemblerImpl +0x120 stream | Resolved 2026-04-26 grooming: old offset from wrong layout. Correct layout: region is `parserCtx_` (AsmParserContext, 0x80B at +0x0f0). |
| 41  | AWGAssemblerImpl +0x148 hash table | Resolved 2026-04-26 grooming: old offset from wrong layout. Correct layout: `labelBimap_` at +0x0a8. |
| 47  | removeUnusedRegs inner scan loop | Resolved Phase 15c: fully reconstructed @0x27e760 (291 lines). Skip bitmask 0x29 documented (skips INVALID, LABEL, cmd=4 — NOT NOP/MESSAGE/ERROR_MSG). |
| 48  | registerAllocation conflict detection | Resolved Phase 15c: structurally reconstructed @0x27ebb0 (1466 lines). 6-phase graph-coloring with backward-branch live-range extension; internal `PhysicalRegister` type confirmed via vector dtor mangled name @0x281840. |
| 49  | splitConstRegisters splitting heuristic | Resolved Phase 15c: structurally reconstructed @0x280440 (444 lines). 2-pass barrier algorithm; `AsmRegister::magicSkipRegister()` @0x28ebb0 returns `{INT_MAX, true}`. |
| 50  | mergeRegisterZeroing: ADDR not XORR | ISA uses ADDI+ADDR for zeroing, not XORR. Documented. |
| 52  | isRead/isWritten: reg2 not checked | Resolved Phase 15c (correction): reg2(+0x20) IS the read source; reg0(+0x28) IS the write destination. Field NAMES in assembler.hpp remain misleading (kept to avoid cascading rename). |
| 53  | getNextActionForReg slot mapping | Resolved Phase 15c (correction): correct 3-field scan (reg2→bit0, reg0→bit1, reg1→3); branch commands return 3 early. |
| 54  | Compiler+0x18 reserved field purpose | Resolved Phase 31a: dead/vestigial field. Write-only — zeroed in ctor and compile(), never read anywhere in the binary. |
| 57  | Expression vs SeqCAstNode relationship | Both reconstructed (11a/11b). parse()→Expression, toSeqCAst()→SeqCAstNode. |
| 58  | getNodeAccessList/getNodeToModeMap returns | Fixed in Phase 11e. Returns `const vector<NodeMapItem>*` / `const unordered_map<..>*`. |
| 59  | compile() inner pipeline steps 10-11, 19 | Resolved Phase 30c/30e: all steps wired, 28/28 differential tests pass. |
| 61  | memoryWrite overlap removal | Resolved Phase 31c: was a no-op stub. Replaced with proper erase loop matching binary (two-case overlap detection + erase + sorted insert) in cache.cpp. |
| 62  | getBestPosition nameMap | Resolved Phase 31c: already correctly implemented. nameMap = "waveforms about to be freed → skip in gap calc". Doc comment added. |
| 63  | allocate splitting heuristic | Resolved Phase 31c: condition was inverted — binary does `if (split \|\| numSamples < freePages) → Normal; else → Aligned+double-buffer`. Fixed in cache.cpp. |
| 64  | kCacheFormat "3" usage | Confirmed Phase 13d: `.format` section in cached ELF stores single byte '3'. cacheFileOutdated checks first byte == '3' to validate cache version. |
| 68  | UsageEntry layout | Resolved Phase 31c: confirmed = PlayConfig (0x20 bytes), no additional fields. No code change needed. |
| 69  | minIndexedSize semantics | Resolved Phase 31c: 4096 = minimum cache size for indexed (wvfi) playback vs direct (wvf). Documented in prefetch.cpp. |
| 77  | Immediate variant dtor vtable | Documented dispatch table for Immediate dtor by discriminant. |
| 81  | placeSingleCommand case label split | Resolved Phase 31c: already fully documented in header comment. Case-1/case-2 merger is structural/refactoring only, no correctness issue. |
| 90  | Exception error_code prefix string | Resolved Phase 31a: prefix is `"ZIException with status code: "` (30 bytes at .rodata 0x90c6c6). Implemented in exception.cpp ctor via `s.insert(0, ...)`. |
| 91  | Exception default ctor sentinel 0x8000 | Resolved Phase 31a: binary uses `make_error_code(0x8000)` with custom `ZiApiErrorCategory` at 0xb7c570. Approximated as `ErrorCode{0x8000}` (value-only, no category needed by seqc consumers). |
| 94  | EDirection/EParamDirection naming | Renamed to EParamDirection in Phase 11b. No further action. |
| 96  | SeqCAstNode type field meaning | Resolved 2026-04-26: `type_` at +0x0C is `lineNr_` (source line number). |
| 97  | lower() return type | Resolved Phase 15a-i: returns `LowerResult` (32B sret = 2 shared_ptrs: `shared_ptr<Node>` from FrontendLoweringState.result + `shared_ptr<EvalResults>` from the evaluate virtual). NOT 16B and NOT 64B (both prior claims were wrong). See `frontend_lowering.md`. |
| 98  | FrontendLoweringState::result type | Resolved Phase 15a-i / confirmed Phase 16e: `shared_ptr<Node>`. Evidence: lower() copies state.result into sret[0]; caller stores into Compiler+0x28 = `shared_ptr<Node> ast_`. Typed in `frontend_lowering.hpp`. |
| 99  | EvalResults struct (0x80 bytes) | Resolved Phase 15a-i: full layout decoded (7 fields, 14 methods). See `frontend_lowering.md`. |
| 100 | EvalResultValue layout (0x38 bytes) | Resolved Phase 15a-i: VarType + VarSubType + Value(embedded 0x28) + AsmRegister. See `frontend_lowering.md`. |
| 101 | CustomFunctions field_168 purpose | Resolved Phase 14a: `std::unordered_set<std::string>` (40B container, 40B node). 1.0f max_load_factor at +0x188 in ctor at 12bec9 confirms unordered_set; node-walk loop at 127d40-127d70 in dtor confirms string elements. Empty in this binary. (Phase 13e initially mis-classified as `vector<T>` — corrected.) |
| 102 | MathCompiler internal layout | Resolved Phase 13e (re-audit): 0x30 bytes = two `std::map`s (libc++ map = 24B each) — single-arg `map<string, function<double(double)>>` at +0x00, multi-arg `map<string, function<double(vector<double> const&)>>` at +0x18. Confirmed via dtor calls 127e7e + 127e64. |
| 103 | AccessMode enum values | Soft=0, Direct=1, Custom=2. Code updated. |
| 104 | NodeMapData subclass field layouts | Resolved Phase 14a: `DirectAddrNodeMapData` (0x10) = vptr + `uint32_t addr_` at +0x08. `VirtAddrNodeMapData` (0x38) = vptr + `std::string name_` at +0x08 + `std::vector<int32_t> addresses_` at +0x20. Confirmed via D0 dtor @0x1c56c0, copy ctor @0x1c4dc0 (vector<int> throw-helper), getJson @0x1c5240 ("name" key write). |
| 106 | warningCallback_ layout gap | libc++ std::function is 0x30 (48B). +0x190+0x30=0x1C0, 8B padding to 0x1C8. |
| 107 | field_20_/field_48_ purpose | libc++ unordered_map max_load_factor (1.0f). Same pattern as CustomFunctions field_80/field_A8. |
| 108 | WaveformGenerator field_50_ | Resolved Phase 13c: renamed `createdNames_` (set<string>) — cache of waveform names already produced by getOrCreateWaveform(). |
| 109 | field_78_ purpose | Padding between shared_ptr<WavetableFront>(16B@+0x68) and function(0x30@+0x80). |
| 110 | WaveformGenerator field_B0_ | Resolved Phase 15a (item #4) as **negative finding**. No setter exists in binary; the 16B slot at +0xB0 is reserved-but-unused (zero-initialized in WG ctor at 0x2482aa, never written elsewhere). |
| 111 | WaveformGenerator aliasMap_ contents | Resolved Phase 13c: empty in this binary. Single ctor write @0x248255 is the member-init `aliasMap_()`; no `insert` calls in WaveformGenerator method range. |
| 113 | CachedFile field ordering | Resolved Phase 13d (correction): layout is `uint16_t channel_; vector<uint8_t> markerBits_; vector<double> samples_; vector<uint8_t> markers_`. NO bool found_ field. |
| 116 | getCachedFile full body | Resolved Phase 13d: lower_bound search, cacheFileOutdated check + erase if stale, ElfReader → 4 sections, LRU touch (timestamp_ + valid_=true). |
| 117 | Resources::addConst stub `st==2` codepath | Resolved Phase 20e-ii Batch 5 prep: value 2 = `VarSubType_FunctionArg`. |
| 118 | Resources::addVar `st==2` codepath | Resolved Phase 20e-ii Batch 5 prep: same as #117 — `VarSubType_FunctionArg`. |
| 119 | SeqCParameter subclass identity | Resolved Phase 21d: "SeqCParameter" does not exist in the binary (`nm -C` shows zero symbols). The +0x14 offset is the `VarType varType_` field of the base class `SeqCAstNode`. Placeholder class removed. |
| 61  | memoryWrite overlap removal | Resolved Phase 31c research: overlap removal was a no-op stub. Fixed with proper erase loop matching binary's memmove-style compaction at 0x283086/0x28310b. |
| 62  | getBestPosition nameMap | Resolved Phase 31c research: nameMap maps waveform names → bool; true = "about to be freed, skip during gap calc." Double-find in binary is artifact. Reconstruction was already correct. |
| 63  | allocate splitting heuristic | Resolved Phase 31c research: condition was inverted. Binary: `if (split \|\| numSamples < freePages) → Normal; else → Aligned chunk with double-buffering`. Fixed. |
| 68  | UsageEntry layout | Resolved Phase 31c research: UsageEntry = PlayConfig wrapper (0x20 bytes), no extra fields. Confirmed from vector stride and field access patterns. |
| 69  | minIndexedSize semantics | Resolved Phase 31c research: threshold (4096 samples) for indexed vs direct waveform playback. >= 4096 → wvfi (indexed, register offset); < 4096 → plain wvf. |

---

## Update history

- **2026-04-26 (Phase 31 completion)**: Closed final 2 items. #45 resolved:
  assembleAsmList register ordering verified (imm→dst→aux→src→out→label),
  cout message fixed to match binary strings. #75 resolved: both cervino
  stubs filled (nonsplit: prf with clampToCache; common finalize split
  per Hirzel/non-Hirzel branch). **0 open unknowns remain.**
- **2026-04-26 (Phase 31d)**: Closed 3 items. #55 resolved: full 0x38-byte
  layout decoded, all methods converted to typed member access, reset() and
  setErrorCallback() added. #93 resolved: pushChild uses standard owning
  shared_ptr, no-op deleter removed. #114 resolved: CacheEntry serializes
  5 fields not 6 (valid_ excluded). Flex/bison entries and callback dispatch
  verified correct. 2 open unknowns remain (#45, #75).
- **2026-04-26 (Phase 31c)**: Closed 7 items. #38 fixed (signal numEntries
  formula). #61 fixed (memoryWrite overlap removal stub). #63 fixed
  (allocate splitting branch inversion). #62 verified correct (getBestPosition
  nameMap). #68 confirmed (UsageEntry=PlayConfig). #69 documented
  (minIndexedSize=4096). #81 resolved (refactoring only).
  2 remain open: #45, #75. 5 total open unknowns.
- **2026-04-26 (Phase 31b)**: Closed 2 items. #27 verified correct.
  #28 fixed (#disableOpt condition inversion in asm_list.cpp).
- **2026-04-26 (Phase 31a)**: Closed 5 items. #90 resolved: Exception
  prefix string is `"ZIException with status code: "` (30 bytes at
  .rodata 0x90c6c6). #91 resolved: sentinel 0x8000 wired into default
  and string ctors via `makeDefaultErrorCode()`. #54 resolved:
  Compiler+0x18 is dead/vestigial (write-only, never read in binary).
  #10 resolved: smap logic after alui is just compiler-generated cleanup
  + two `st()` calls already implemented. #32 resolved: DeviceConstants
  anonymous enums already fully documented (SyncRegA=0x44, SyncRegB=0x45).
  (#18, #23-26, #39-41, #59, #119) that were stale — their blockers had
  been cleared by Phases 21h/22d (evaluate), 30c/30e (pipeline), and
  layout re-derivations. Emptied Blocked, Actionable, and Deferred
  sections — all 19 remaining open items promoted to TODO.md Phase 31
  as actionable work items. No more "deferred" or "low value" categories.
- **2026-04-26**: #96 resolved. SeqCAstNode `type_` at +0x0C is a **source
  line number** (`lineNr_`).
- **2026-04-24 (Phase 20e-ii Sub-phase 5b wrap-up)**: #119 added (Blocked).
- **2026-04-24 (Phase 20e-ii Batch 5a wrap-up cleanup)**: removed
  duplicate "Carry-forward items from Phase 15c" section.
- **2026-04-24 (Phase 20e-ii Batch 5 prep)**: #117, #118 added and closed.
- **2026-04-23 (Phase 15c)**: #47, #48, #49 closed.
- **2026-04-23 (Phase 15a-i)**: #97, #99, #100 closed. #110 closed as
  negative finding.
- **2026-04-23 (Phase 14a)**: #101, #104 closed.
- **2026-04-23 (Phase 13e re-audit)**: #102 closed.
- **2026-04-23 (Phase 13d wrap-up)**: #113, #116 closed; #64 cross-referenced.
- **2026-04-23 (Phase 13c wrap-up)**: #108, #111 closed.
- **2026-04-23 (Notes restructure)**: file renamed from `unknowns_open.md`.
- **2026-04-22 (initial triage)**: extracted from `archive/unknowns_full_1-116.md`.
