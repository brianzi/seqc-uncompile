# Unknowns

Working registry of open questions and historical resolutions for the
zhinst SeqC compiler reverse-engineering effort.

This file holds:
- **Closed** items: questions answered, with the resolution method noted.
- **Actionable** items: ready for disassembly work, folded into TODO.md phases.
- **Blocked** items: known to require prerequisites first.
- **Deferred** observations: documented findings that need no code change.

Original item numbers (1-116) are preserved for cross-reference from code
comments (`// see unknowns.md #97`). Numbers ≥117 are post-archive additions.
The full pre-2026-04-22 history is preserved in
[`archive/unknowns_full_1-116.md`](archive/unknowns_full_1-116.md).

---

## Closed

| #   | Title | Resolution |
|-----|-------|-----------|
| 47  | removeUnusedRegs inner scan loop | Resolved Phase 15c: fully reconstructed @0x27e760 (291 lines). Skip bitmask 0x29 documented (skips INVALID, LABEL, cmd=4 — NOT NOP/MESSAGE/ERROR_MSG). |
| 48  | registerAllocation conflict detection | Resolved Phase 15c: structurally reconstructed @0x27ebb0 (1466 lines). 6-phase graph-coloring with backward-branch live-range extension; internal `PhysicalRegister` type confirmed via vector dtor mangled name @0x281840. |
| 49  | splitConstRegisters splitting heuristic | Resolved Phase 15c: structurally reconstructed @0x280440 (444 lines). 2-pass barrier algorithm; `AsmRegister::magicSkipRegister()` @0x28ebb0 returns `{INT_MAX, true}`. |
| 50  | mergeRegisterZeroing: ADDR not XORR | ISA uses ADDI+ADDR for zeroing, not XORR. Documented. |
| 52  | isRead/isWritten: reg2 not checked | Resolved Phase 15c (correction): reg2(+0x20) IS the read source; reg0(+0x28) IS the write destination. Field NAMES in assembler.hpp remain misleading (kept to avoid cascading rename). |
| 53  | getNextActionForReg slot mapping | Resolved Phase 15c (correction): correct 3-field scan (reg2→bit0, reg0→bit1, reg1→3); branch commands return 3 early. |
| 57  | Expression vs SeqCAstNode relationship | Both reconstructed (11a/11b). parse()→Expression, toSeqCAst()→SeqCAstNode. |
| 58  | getNodeAccessList/getNodeToModeMap returns | Fixed in Phase 11e. Returns `const vector<NodeMapItem>*` / `const unordered_map<..>*`. |
| 64  | kCacheFormat "3" usage | Confirmed Phase 13d: `.format` section in cached ELF stores single byte '3'. cacheFileOutdated checks first byte == '3' to validate cache version. |
| 77  | Immediate variant dtor vtable | Documented dispatch table for Immediate dtor by discriminant. |
| 94  | EDirection/EParamDirection naming | Renamed to EParamDirection in Phase 11b. No further action. |
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
| 110 | WaveformGenerator field_B0_ | Resolved Phase 15a (item #4) as **negative finding**. No setter exists in binary; the 16B slot at +0xB0 is reserved-but-unused (zero-initialized in WG ctor at 0x2482aa, never written elsewhere). Apparent +0xb0/+0xb8 reads inside WG methods (251973, 25385a, 255068, ...) were misattributed Value-union accesses (Value has type tag at +0xA8 and 16B union storage at +0xB0). The single true +0xb0 write at 0x11d180 is `Compiler::Compiler` writing its own AsmCommands shared_ptr. Likely a feature compiled out, or a hook never installed by the dynamic loader. |
| 111 | WaveformGenerator aliasMap_ contents | Resolved Phase 13c: empty in this binary. Single ctor write @0x248255 is the member-init `aliasMap_()`; no `insert` calls in WaveformGenerator method range. |
| 113 | CachedFile field ordering | Resolved Phase 13d (correction): layout is `uint16_t channel_; vector<uint8_t> markerBits_; vector<double> samples_; vector<uint8_t> markers_`. NO bool found_ field. Sections read in cacheFile/getCachedFile: .channels (channel_), .marker_bits, .data, .marker. |
| 116 | getCachedFile full body | Resolved Phase 13d: lower_bound search, cacheFileOutdated check + erase if stale, ElfReader → 4 sections, LRU touch (timestamp_ + valid_=true). |
| 117 | Resources::addConst stub `st==2` codepath | Resolved Phase 20e-ii Batch 5 prep: value 2 = `VarSubType_FunctionArg`. `Function::addArgument` @0x1e9f60 calls each add* method (addVar/addCvar/addConst/addWave/addString) on the inner scope with `edx=0x2` to bind a parameter. Pre-marks the variable as written (flags+0x50 = 1) and sets the +0x51 frozen byte that makes update* short-circuit value reassignment. Enum extended in resources.hpp. |
| 118 | Resources::addVar `st==2` codepath | Resolved Phase 20e-ii Batch 5 prep: same as #117 — `VarSubType_FunctionArg`. The "pre-mark as written" path is the parameter-binding path used by `Function::addArgument`. |

---

## Actionable — folded into TODO.md phases

### Pipeline / lowering

10\. **smap remaining logic** — ~0x1E6 bytes after alui call.

---

## Blocked — need prerequisites

| # | Title | Prerequisite |
|---|-------|--------------|
| 18 | AssemblerInstr outputs population | Needs expression evaluation disassembly. |
| 54 | Compiler+0x18 reserved field | Needs deeper pipeline analysis. |
| 55 | SeqcParserContext layout | Needs parser context disassembly. |
| 59 | compile() inner pipeline steps 10-11, 19 | Needs full pipeline trace. |
| 119 | ~~SeqCParameter subclass identity~~ **RESOLVED Phase 21d** | Phase 21d discovered that "SeqCParameter" does not exist in the binary (`nm -C` shows zero symbols). The +0x14 offset is the `VarType varType_` field of the base class `SeqCAstNode`, now properly declared. The +0x18 offset is `SeqCVariable::name_`. `addArguments(SeqCAstNode const&)` updated to use `node.varType()` and `static_cast<SeqCVariable const*>(node)->name()`. SeqCParameter class removed from `seqc_ast_node.hpp`. |

---

## Deferred observations (documented, no code impact)

- **Phase 2-3 semantics**: #23 (Node deviceIndex naming), #24 (Node play
  field), #25 (Node length field), #26 (toJson idMap), #27 (serialize
  opcode==4 skip), #28 (serialize #disableOpt), #32 (DeviceConstants
  anonymous enums), #38 (Signal marker bit distribution).
- **AWGAssemblerImpl internals**: #39 (+0x20 string), #40 (+0x120 stream),
  #41 (+0x148 hash table), #45 (assembleAsmList register ordering).
- **Cache/Prefetch detail**: #61 (memoryWrite overlap removal),
  #62 (getBestPosition nameMap), #63 (allocate splitting heuristic),
  #68 (UsageEntry layout), #69 (minIndexedSize), #75 (cervino indexed
  nonsplit), #81 (placeSingleCommand case label split).
- **Exception**: #90 (error_code prefix string), #91 (default ctor
  sentinel 0x8000).
- **Expression**: #93 (pushChild ownership model), ~~#96 (SeqCAstNode type
  field meaning)~~ RESOLVED.
- **CachedParser**: #114 (CacheEntry serialize template body).

---

## Update history

- **2026-04-26**: #96 resolved. SeqCAstNode `type_` at +0x0C is a **source
  line number** (`lineNr_`). Used as `int lineNr = type_` in 37+ evaluate
  overrides and printed as `"(line: ...)"` by `printSeqCAst`. Renamed
  `type_` → `lineNr_`. Note: `SeqCVariable::print()` casts it to VarType
  for display — overloaded meaning in that one subclass only.
  does not exist in the binary. VarType is base-class field at +0x14;
  +0x18 is SeqCVariable::name_. Placeholder class removed.
- **2026-04-24 (Phase 20e-ii Sub-phase 5b wrap-up)**: #119 added (Blocked).
  SeqCParameter subclass identity — discovered during
  `Function::addArguments(SeqCAstNode)` reconstruction. Placeholder class
  added to seqc_ast_node.hpp with a `varType()` accessor reading +0x14.
- **2026-04-24 (Phase 20e-ii Batch 5a wrap-up cleanup)**: removed
  duplicate "Carry-forward items from Phase 15c" section. The trio
  (simplifyAssign, splitReg, register-field rename) is the actionable
  list in `TODO.md` (Deferred / Low Priority) and the technical
  reference is `notes/optimization_passes.md` (sections "simplifyAssign
  (0x280e10)" line 96, "splitReg (0x281000)" line 137, and the
  register-semantics correction discussion at line 200). Maintained as
  two-sources-of-truth per AGENTS.md (TODO.md = actionable; notes/ =
  technical detail).

- **2026-04-24 (Phase 20e-ii Batch 5 prep)**: #117, #118 added and closed
  in same turn. `VarSubType_FunctionArg = 2` enum value identified from
  Function::addArgument disasm at 0x1e9fae (and four sibling sites). The
  add*(name, st) stub overloads' previously-unknown st==2 codepath is the
  parameter-binding path — pre-marks `flags[+0x50]=1` and sets the
  `flags[+0x51]` frozen byte.
- **2026-04-23 (Phase 15c)**: #47, #48, #49 closed. Carry-forward items
  added (simplifyAssign, splitReg, register field rename).
- **2026-04-23 (Phase 15a-i)**: #97, #99, #100 closed. #110 closed as
  negative finding.
- **2026-04-23 (Phase 14a)**: #101, #104 closed (corrections to Phase 13e
  re-audit).
- **2026-04-23 (Phase 13e re-audit)**: #102 closed.
- **2026-04-23 (Phase 13d wrap-up)**: #113, #116 closed; #64 cross-referenced.
- **2026-04-23 (Phase 13c wrap-up)**: #108, #111 closed.
- **2026-04-23 (Notes restructure)**: file renamed from `unknowns_open.md`
  to `unknowns.md`; the old redirect stub at `unknowns.md` was deleted.
  Format consolidated; Closed/Actionable/Blocked/Deferred sections
  cleaned of duplicates and stale claims.
- **2026-04-22 (initial triage)**: extracted from `archive/unknowns_full_1-116.md`.
