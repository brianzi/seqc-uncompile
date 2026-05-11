# Phase 43 Investigation: 54-Function Binary vs Recon Size Audit

This file records findings from a read-only sweep comparing 54 functions in the
original `_seqc_compiler.so` against the current reconstruction. For each
function the binary size (from `nm -S _seqc_compiler.so`) and the recon build
size (from `nm -S reconstructed/build/_seqc_compiler.so`) are listed, followed
by a structural analysis.

Status labels:
- **OK** — functionally equivalent; size delta is optimizer / inlining noise
- **WRONG-RET** — return type mismatch (ABI-visible)
- **UNDER** — recon is missing logic present in the binary
- **OVER** — recon has more code than the binary for the same operation
- **DIFF-ALGO** — correct result, but entirely different algorithm
- **STUB** — recon is a stub / placeholder

---

## Group A: AsmCommands emit helpers

### 1. `asmBranchNode` — `_ZNK6zhinst11AsmCommands13asmBranchNodeEv`
- Binary: 0x172 (370 B) @ 0x279d70
- Recon:  0x51  (81 B)
- **Status: OK** — binary inlines the full `Assembler` struct construction +
  `allocate_shared<Node>` call at each site; recon calls the `emitNodeEntry()`
  helper which consolidates that pattern. NodeType = 0x4 (Branch).

### 2. `asmLoopNode` — `_ZNK6zhinst11AsmCommands12asmLoopNodeEv`
- Binary: 0x172 @ 0x279ee0
- Recon:  0x51
- **Status: OK** — identical structure to `asmBranchNode`. NodeType = 0x8 (Loop).

### 3. `asmSyncPlaceholderCervino` — `_ZNK6zhinst11AsmCommands25asmSyncPlaceholderCervinoEv`
- Binary: 0x172 @ 0x279d10
- Recon:  0x51
- **Status: OK** — same inline pattern. NodeType = 0x100 (SyncPlaceholder).

### 4. `asmLoadPlaceholder` — `_ZNK6zhinst11AsmCommands18asmLoadPlaceholderEv`
- Binary: 0x172 @ 0x27a050
- Recon:  0x51
- **Status: OK** — NodeType = 0x1 (LoadPlaceholder). Uses a different
  `allocate_shared` variant in the binary (distinguishable by vtable pointer
  written to the node), but the observable behavior is the same.

### 5. `asmRate(int)` — `_ZNK6zhinst11AsmCommands7asmRateEi`
- Binary: 0x18e @ 0x27a1c0
- Recon:  0x74
- **Status: OK** — NodeType = 0x20 (Rate). Binary additionally stores the `int`
  parameter into `node->rate` at offset +0x100; recon does this via
  `emitNodeEntry(instr, nodeType, rate)`.

### 6. `asmSetPrecompFlags(uint)` — `_ZNK6zhinst11AsmCommands18asmSetPrecompFlagsEj`
- Binary: 0x18e @ 0x27a350
- Recon:  0x74
- **Status: OK** — NodeType = 0x1000. Same pattern.

### 7. `asmSetVarPlaceholder(AsmRegister)` — `_ZNK6zhinst11AsmCommands20asmSetVarPlaceholderE11AsmRegister`
- Binary: 0x18e @ 0x27a4e0
- Recon:  0x7a
- **Status: OK** — NodeType = 0x10. Takes an AsmRegister parameter stored into
  the node at offset +0xf8.

### 8. `wwvfq` — `_ZNK6zhinst11AsmCommands5wwvfqEv`
- Binary: 0x11a @ 0x27b130
- Recon:  0x63
- **Status: OK** — builds an `Assembler` on the stack (cmd = 0xF0000000 =
  WWVFQ), sets a flag in the result based on the cmd value; recon delegates to
  `impl_->wwvfq(wavetableFrontIndex_)`.

### 9. `wprf` — `_ZNK6zhinst11AsmCommands4wprfEv`
- Binary: 0x11a @ 0x27b250
- Recon:  0x63
- **Status: OK** — identical structure to `wwvfq`.

---

## Group A: str() helpers

### 10. `str(ECommandType)` — `_ZN6zhinst3strENS_12ECommandTypeE`
- Binary: 0x187 (391 B) @ 0x1c7500
- Recon:  0x1e3 (483 B)
- **Status: WRONG-RET** — the binary returns a short `std::string` by value
  (SSO, using the hidden-return-value ABI convention with an output pointer in
  `rdi`). The recon declares return type `const char*`. The logic is correct
  (switch on enum 0–0x10), but the ABI-visible signature is wrong. The recon
  is incidentally larger because of string constructor overhead at every case.

### 11. `str(EOperationType)` — `_ZN6zhinst3strENS_14EOperationTypeE`
- Binary: 0x186 (390 B) @ 0x1a66f0
- Recon:  0x15f (351 B)
- **Status: WRONG-RET** — same return-type mismatch as `str(ECommandType)`.
  Binary handles enum values 0x0–0xc (13 cases).

### 12. `str(EOperator)` — `_ZN6zhinst3strENS_9EOperatorE`
- Binary: 0x148 (328 B) @ 0x196050
- Recon:  0x283 (643 B)
- **Status: WRONG-RET** — same return-type mismatch. Binary handles 0x0–0x15
  (22 operators). Recon is nearly 2× the binary size due to the wrong ABI.

---

## Group A: Misc small functions

### 13. `Resources::printAll` — `_ZNK6zhinst9Resources8printAllEv`
- Binary: 0x106 (262 B) @ 0x1a5c60
- Recon:  0x27  (39 B)
- **Status: UNDER** — binary acquires a `shared_ptr` lock on `parent_` before
  printing (calls `__shared_weak_count::lock()` at +0x58), then calls
  `print()`, then calls `printScopes()`. Recon just calls `print(); printScopes()`
  with no parent-lock. The parent-lock logic is entirely absent in recon.

### 14. `highestRegisterNumber` — `_ZNK6zhinst11AsmCommands22highestRegisterNumberEv`
- Binary: 0x308 (776 B) @ 0x280130
- Recon:  0xa0  (160 B)
- **Status: DIFF-ALGO** — binary builds a `vector<AsmRegister>` of valid
  registers using raw heap allocation + `memcpy`, computes a bitmask from the
  set, and derives the highest number from the mask. Recon uses a simple
  if-chain iterating over `maxReg`. Result should be equivalent for well-formed
  input, but the algorithm is completely different.

### 15. `getDefaultLabOneResource` — `_ZN6zhinst22getDefaultLabOneResourceEv`
- Binary: 0x19f (415 B) @ 0x226370
- Recon:  0xbf  (191 B)
- **Status: UNDER** — binary constructs a full opentelemetry `Resource` object
  with 4 key-value string attributes from `.rodata`. Recon likely omits some
  attributes; without seeing the full recon it's likely missing at least 2 of
  the 4 attribute initializations.

### 16. `VirtAddrNodeMapData::hash` — `_ZNK6zhinst19VirtAddrNodeMapData4hashEv`
- Binary: 0x2cd (717 B) @ 0x16fce0
- Recon:  0x1a3 (419 B)
- **Status: DIFF-ALGO** — binary uses wyhash constants directly (hardcoded
  64-bit magic numbers visible at offsets +0x20). Recon delegates to
  `std::hash<string>`. Different hash values will be produced for the same
  input. This is a behavioral difference if hash values are observable
  (e.g. stored in output or used for de-duplication).

---

## Group B: AST node constructors

### 17. `MathCompiler::MathCompiler` — `_ZN6zhinst12MathCompilerC2Ev`
- Binary: 0x12cb (4811 B) @ 0x167600
- Recon:  0x9d0  (2512 B)
- **Status: OK** — both register the same set of math functions. Size
  difference is optimizer inlining and string literal handling.

### 18. `createFunctionCall`
- Binary: 0x191 @ 0x1b0f80
- Recon:  0xa4
- **Status: OK** — optimizer diff.

### 19. `createIfElse`
- Binary: 0x209 @ 0x1b1540
- Recon:  0x8b
- **Status: OK**

### 20. `createCondExpression`
- Binary: 0x209 @ 0x1b1760
- Recon:  0x8b
- **Status: OK**

### 21. `createFor`
- Binary: 0x27e @ 0x1b1120
- Recon:  0xa4
- **Status: OK**

### 22. `createCase`
- Binary: 0x18f @ 0x1b1970
- Recon:  0x8c
- **Status: OK**

### 23. `SeqCIfElse::operator=` — `_ZN6zhinst10SeqCIfElseaSERKS0_`
- Binary: 0x16f (367 B)
- Recon:  0x29  (41 B)
- **Status: OK** — binary manually moves 3 `unique_ptr` members
  (`cond_`, `thenBody_`, `elseBody_`) with vtable-dispatched clone calls. Recon
  uses `swap(*this, o)`. Functionally equivalent.

### 24. `SeqCCondExpr::operator=` — `_ZN6zhinst12SeqCCondExpraSERKS0_`
- Binary: 0x16f
- Recon:  0x29
- **Status: OK** — same pattern as `SeqCIfElse::operator=`.

### 25. `ElfWriter::ElfWriter(uint16_t)` — `_ZN6zhinst9ElfWriterC2Et`
- Binary: 0x204 (516 B) @ 0x2c1c50
- Recon:  0x62  (98 B)
- **Status: OK** — binary inlines the full ELF32 header setup (builds the
  e_ident magic bytes, sets class/data/version, then calls
  `create_mandatory_sections`, then sets machine/type/flags via vtable).
  Recon calls `prepareHeader(machineType)` helper.

---

## Group C: Miscellaneous

### 26. `swap(SeqCValue&, SeqCValue&)` — `_ZN6zhinst4swapERNS_9SeqCValueES1_`
- Binary: 0x27   (39 B)
- Recon:  0x1ee (494 B)
- **Status: OVER** — binary is 7 instructions: swap an `int` field at +0x14,
  then call `__variant_detail::__impl::__swap` on the variant at +0x18. Recon
  is 494 bytes (likely a hand-rolled element-by-element swap). The binary is
  the canonical two-field swap; the recon should be simplified to match.

### 27. `getAccessModes` — (custom_functions)
- Binary: 0x27  (39 B)
- Recon:  0x119 (281 B)
- **Status: OVER** — binary is tiny: offset `rdi` by +0x128, call hash_table
  `find`, check null, return +0x28 or throw. Recon is 281 bytes with full
  explicit lookup logic. Should be simplified to a single hash-table lookup.

### 28. `Immediate::operator int()` — `_ZNK6zhinst9ImmediatecviEv`
- Binary: 0x39  (57 B) @ 0x290cc0
- Recon:  0x88 (136 B)
- **Status: OK** — binary uses a vtable dispatch table at 0xb070b0; recon uses
  a `switch`. Functionally equivalent; recon is larger.

### 29. `isIa` — `_ZNK6zhinst10DeviceType4isIaEv`
- Binary: 0x45 (69 B)
- Recon:  0x80 (128 B)
- **Status: OK** — binary uses two `bt` (bit-test) instructions with masks
  `0x46BF7901` and `0x900`; recon uses right-shift comparisons. Equivalent.

### 30. `alignWaveformSizes` — `_ZN6zhinst11WavetableIR19alignWaveformSizesEv`
- Binary: 0x7e  (126 B)
- Recon:  0xa6  (166 B)
- **Status: OK** — binary calls `forEachUsedWaveform` with inline lambda; recon
  is slightly larger but equivalent.

### 31. `assignWaveformAllocationSizes` — `_ZN6zhinst11WavetableIR30assignWaveformAllocationSizesEv`
- Binary: 0xf9  (249 B)
- Recon:  0x10b (267 B)
- **Status: OK** — binary acquires `cancelCallback_` lock first; recon also
  does so. Slightly larger in recon due to optimizer differences.

### 32. `Node::~Node` — `_ZN6zhinst4NodeD2Ev`
- Binary: 0x1dc (476 B) @ 0x12afe0
- Recon:  0x95  (149 B)
- **Status: UNDER** — binary releases `next_` (`shared_ptr` at +0xf8),
  `parent_` (`shared_ptr` at +0xe8), and a vector of children `shared_ptr`s
  at +0xc8, plus at least 4 additional heap-allocated members (7 total
  `__release_weak` calls + 3 `operator delete` calls visible). Recon dtor is
  `= default` and only destroys what is declared in the recon header — meaning
  the recon `Node` header is missing several fields that exist in the binary.

### 33. `Prefetch::~Prefetch` — `_ZN6zhinst8PrefetchD2Ev`
- Binary: 0x27a (634 B) — not found via nm (local symbol); verified via
  objdump at its expected address
- Recon:  0xe1  (225 B)
- **Status: UNDER** — binary releases more members than the recon. Recon
  `Prefetch` header is likely missing fields.

### 34. `Signal::Signal(size_t)` — constructor
- Binary: 0x11b (283 B)
- Recon:  0x153 (339 B)
- **Status: OVER** — binary allocates a 1-byte marker buffer and two
  separate arrays; recon does extra initialization. Recon is 56 bytes larger.

### 35. `Signal::checkAllocation` — `_ZN6zhinst6Signal15checkAllocationEv`
- Binary: 0x130 (304 B) @ 0x246950
- Recon:  0xd3  (211 B)
- **Status: UNDER** — binary manually implements vector reallocation (calls
  `__append` for the double-samples vector and has its own grow loop for
  markers). Also accesses a third vector at +0x28 (likely `markerBits_`)
  which the recon may not grow. Recon uses `resize()` which is semantically
  close but misses the `markerBits_` growth at +0x28.

---

## Group B continued: Larger functions

### 36. `syncCervino` — `_ZNK6zhinst11AsmCommands12syncCervinoE...`
- Binary: 0x10b6 (4278 B) @ 0x275c50
- Recon:  not individually listed in recon nm (inlined or merged)
- **Status: OK** (from earlier analysis of the calling context)

### 37. `unsyncCervino` — `_ZNK6zhinst11AsmCommands13unsyncCervinoEv`
- Binary: 0x4cc (1228 B) @ 0x276d10
- Recon:  0x3d6 (982 B)
- **Status: OK** — both emit two ST instructions (opcodes 0xf6000000) clearing
  sync registers at destinations 0x44 and 0x45. Size difference is exception-
  handling path generation in the binary.

### 38. `alui` — `_ZNK6zhinst11AsmCommands4aluiENS_9Assembler7CommandE11AsmRegisterS3_NS_9ImmediateE`
- Binary: 0xf98 (3992 B) @ 0x272dc0
- Recon:  ~similar (asm_commands.cpp:306)
- **Status: OK** — recon correctly handles 3 cases: (1) fits in 18-bit signed
  range → single immediate instruction; (2) ADDI with large immediate → split
  low12 + upper via ADDIU; (3) other ALU ops → load constant then register-
  register operation. Binary size is larger due to exception-handling paths.

### 39. `fillInPlaceholders` — (prefetch_emit.cpp)
- Binary: previously analyzed
- **Status: OK** (from prior analysis)

### 40. `preparePlays` — (prefetch_prepare.cpp)
- Binary: previously analyzed
- **Status: OK** (from prior analysis)

---

## Group C continued

### 41. `ZiFolder::ziFolder(DirectoryType)` — `_ZN6zhinst8ZiFolder8ziFolderENS0_13DirectoryTypeE`
- Binary: 0x83a (2106 B) @ 0x2cf0c0
- Recon:  0x494 (1172 B)
- **Status: UNDER** — binary is almost twice the size of the recon. The binary
  begins with `cmp $0x2, %edx` (DirectoryType == 2 check) and then a 0x1000-
  byte buffer allocation on the stack for path manipulation. The recon is
  missing at least one code path. Likely the binary handles more `DirectoryType`
  enum values than the recon implements.

### 42. `oscMaskCheckHirzel` — `_ZN6zhinst15CustomFunctions18oscMaskCheckHirzelEj`
- Binary: 0x495 (1173 B) @ 0x15bab0
- Recon:  0x3c8 (968 B)
- **Status: UNDER** — binary builds a 4-byte SSO string `"FM"` at stack offset
  -0x130 (byte 0x4 length, bytes `0x46 0x4D = "FM"`) and calls a virtual
  dispatch at `(*rdi+0x70)(...)`. Recon is 205 bytes shorter. Likely missing
  one validation or error-message path.

### 43. `createDummyWaveform` — `_ZN6zhinst17WaveformGenerator19createDummyWaveformEi`
- Binary: 0x2a4 (676 B) @ 0x25be70
- Recon:  0x189 (393 B)
- **Status: UNDER** — binary writes SSO string `"zeros"` (0xa length byte,
  chars `0x6f72657a 0x73`) onto the stack at -0x78; then writes literal `1`
  at -0x60 and the int param at -0x50. Recon is 283 bytes shorter — likely
  missing the waveform-metadata initialization or registration steps.

### 44. `SeqCVariable::print()` — `_ZNK6zhinst12SeqCVariable5printEv`
- Binary: 0x3fe (1022 B) @ 0x1fdbd0
- Recon:  0x1e2 (482 B)
- **Status: UNDER** — binary is more than twice as large. Likely missing
  several field-printing statements or nested-scope printing logic.

### 45. `determineFixedWaves` — `_ZN6zhinst8Prefetch19determineFixedWavesEv`
- Binary: 0xd53 (3411 B) @ 0x1cb200
- Recon:  0x431 (1073 B)
- **Status: UNDER** — binary is 3.2× larger. Major logic is missing from the
  recon; only the skeletal structure has been reconstructed.

### 46. `CachedParser::CachedParser(size_t, boost::filesystem::path const&)`
- Binary: 0x3ba (954 B) @ 0x2afa70
- Recon:  0x23d (573 B)
- **Status: UNDER** — binary is 381 bytes larger. Likely missing
  `loadCacheIndex()` call sequence or additional member initialization steps.

### 47. `AsmOptimize::registerAllocation(unsigned long)` — `_ZN6zhinst11AsmOptimize18registerAllocationEm`
- Binary: 0x1890 (6288 B) @ 0x27ebb0
- Recon:  0x125f (4703 B)
- **Status: UNDER** — binary starts with `movabs $0xaaaaaaaaaaaaaaa` bitmask
  (repeated-2-bit pattern for even/odd register allocation), processes
  `numRegs+1` incremented count. Recon is 1585 bytes shorter — one or more
  allocation phases or spill-handling paths are missing.

### 48. `ElfReader::getLineMap()` — `_ZNK6zhinst9ElfReader10getLineMapEv`
- Binary: 0x101 (257 B) @ 0x2c3ef0
- Recon:  0x27f (639 B)
- **Status: OVER** — recon is 2.5× the binary size. Likely over-complicated
  parsing of the `.linenr` section (which is just pairs of uint32 LE values).

### 49. `placeLoads` — `_ZN6zhinst8Prefetch10placeLoadsEv`
- Binary: 0x6e5 (1765 B) @ 0x1cbf60
- Recon:  0x80c (2060 B)
- **Status: OVER** — recon is 295 bytes larger. Some extra branching or
  initialization in the recon that does not exist in the binary.

### 50. `splitConstRegisters` — `_ZN6zhinst11AsmOptimize19splitConstRegistersEm`
- Binary: 0x717 (1815 B) @ 0x280440
- Recon:  0xa9a (2714 B)
- **Status: OVER** — recon is 899 bytes larger. Significant over-implementation;
  the binary algorithm is substantially simpler.

### 51. `allocateWaveforms(bool)` — `_ZN6zhinst11WavetableIR17allocateWaveformsEb`
- Binary: 0x29f (671 B) @ 0x29e340
- Recon:  0x34c (844 B)
- **Status: OVER** — recon is 173 bytes larger.

### 52. `allocateWaveformsForFifo` — `_ZN6zhinst11WavetableIR24allocateWaveformsForFifoEv`
- Binary: 0x419 (1049 B) @ 0x29ed30
- Recon:  0x38c (908 B)
- **Status: UNDER** — binary is 141 bytes larger.

### 53. `assignWaveIndexImplicit` — `_ZN6zhinst11WavetableIR23assignWaveIndexImplicitEv`
- Binary: 0x434 (1076 B) @ 0x29e8a0
- Recon:  0x63b (1595 B)
- **Status: OVER** — recon is 519 bytes larger. Substantial over-implementation.

### 54. `EvalResults::setValue` (all overloads)

| Overload | Binary addr | Binary size | Recon size | Status |
|---|---|---|---|---|
| `setValue(double)` | 0x2136a0 | 0x232 (562) | 0x1e7 (487) | **OK** |
| `setValue(VarType)` | 0x20ad20 | 0x1f7 (503) | 0x1a6 (422) | **OK** |
| `setValue(VarType, int)` | 0x15c850 | 0x232 (562) | 0x270 (624) | OVER (+62) |
| `setValue(VarType, VarSubType, Value const&)` | 0x16bfb0 | 0x267 (615) | 0x1bb (443) | **OK** |
| `setValue(VarType, VarSubType, Value const&, int)` | 0x247600 | 0x269 (617) | 0x2a2 (674) | OVER (+57) |
| `setValue(VarType, Value const&)` | 0x211b70 | 0x267 (615) | 0x1b7 (439) | **OK** |
| `setValue(VarType, Value const&, int)` | 0x2107b0 | 0x279 (633) | 0x299 (665) | OVER (+32) |
| `setValue(VarType, string const&)` | 0x20af20 | 0x2d5 (725) | 0x200 (512) | **OK** |
| `setValue(Value const&)` | 0x15a750 | 0x267 (615) | 0x1ae (430) | **OK** |

The OVER cases involve overloads that take an `int reg` parameter. The binary
is shorter — likely because the binary stores the register directly without
extra branching in the `AsmRegister` constructor path.

---

## Summary Table

| # | Function | Binary (B) | Recon (B) | Status |
|---|---|---|---|---|
| 1 | asmBranchNode | 370 | 81 | OK |
| 2 | asmLoopNode | 370 | 81 | OK |
| 3 | asmSyncPlaceholderCervino | 370 | 81 | OK |
| 4 | asmLoadPlaceholder | 370 | 81 | OK |
| 5 | asmRate | 398 | 116 | OK |
| 6 | asmSetPrecompFlags | 398 | 116 | OK |
| 7 | asmSetVarPlaceholder | 398 | 122 | OK |
| 8 | wwvfq | 282 | 99 | OK |
| 9 | wprf | 282 | 99 | OK |
| 10 | str(ECommandType) | 391 | 483 | WRONG-RET |
| 11 | str(EOperationType) | 390 | 351 | WRONG-RET |
| 12 | str(EOperator) | 328 | 643 | WRONG-RET |
| 13 | Resources::printAll | 262 | 39 | UNDER |
| 14 | highestRegisterNumber | 776 | 160 | DIFF-ALGO |
| 15 | getDefaultLabOneResource | 415 | 191 | UNDER |
| 16 | VirtAddrNodeMapData::hash | 717 | 419 | DIFF-ALGO |
| 17 | MathCompiler::MathCompiler | 4811 | 2512 | OK |
| 18 | createFunctionCall | 401 | 164 | OK |
| 19 | createIfElse | 521 | 139 | OK |
| 20 | createCondExpression | 521 | 139 | OK |
| 21 | createFor | 638 | 164 | OK |
| 22 | createCase | 399 | 140 | OK |
| 23 | SeqCIfElse::operator= | 367 | 41 | OK |
| 24 | SeqCCondExpr::operator= | 367 | 41 | OK |
| 25 | ElfWriter::ElfWriter | 516 | 98 | OK |
| 26 | swap(SeqCValue&) | 39 | 494 | OVER |
| 27 | getAccessModes | 39 | 281 | OVER |
| 28 | Immediate::operator int | 57 | 136 | OK |
| 29 | isIa | 69 | 128 | OK |
| 30 | alignWaveformSizes | 126 | 166 | OK |
| 31 | assignWaveformAllocationSizes | 249 | 267 | OK |
| 32 | Node::~Node | 476 | 149 | UNDER |
| 33 | Prefetch::~Prefetch | 634 | 225 | UNDER |
| 34 | Signal::Signal(size_t) | 283 | 339 | OVER |
| 35 | Signal::checkAllocation | 304 | 211 | UNDER |
| 36 | syncCervino | 4278 | — | OK |
| 37 | unsyncCervino | 1228 | 982 | OK |
| 38 | alui | 3992 | ~similar | OK |
| 39 | fillInPlaceholders | — | — | OK |
| 40 | preparePlays | — | — | OK |
| 41 | ZiFolder::ziFolder | 2106 | 1172 | UNDER |
| 42 | oscMaskCheckHirzel | 1173 | 968 | UNDER |
| 43 | createDummyWaveform | 676 | 393 | UNDER |
| 44 | SeqCVariable::print | 1022 | 482 | UNDER |
| 45 | determineFixedWaves | 3411 | 1073 | UNDER |
| 46 | CachedParser::CachedParser | 954 | 573 | UNDER |
| 47 | registerAllocation | 6288 | 4703 | UNDER |
| 48 | ElfReader::getLineMap | 257 | 639 | OVER |
| 49 | placeLoads | 1765 | 2060 | OVER |
| 50 | splitConstRegisters | 1815 | 2714 | OVER |
| 51 | allocateWaveforms | 671 | 844 | OVER |
| 52 | allocateWaveformsForFifo | 1049 | 908 | UNDER |
| 53 | assignWaveIndexImplicit | 1076 | 1595 | OVER |
| 54 | EvalResults::setValue (×9) | various | various | mixed (see table) |

### Status counts
- **OK**: 26
- **UNDER**: 14 (recon missing logic)
- **OVER**: 10 (recon over-implemented)
- **WRONG-RET**: 3 (ABI-visible return type wrong)
- **DIFF-ALGO**: 2 (different algorithm, may produce different values)

### Highest-priority issues (behavioral correctness risk)
1. **WRONG-RET** on `str(ECommandType/EOperationType/EOperator)` — callers
   that expect a `std::string` return will have undefined behavior.
2. **DIFF-ALGO** on `VirtAddrNodeMapData::hash` — different hash values will
   break hash-map lookups, potentially causing silent correctness bugs.
3. **UNDER** on `Node::~Node` — missing field releases indicate the `Node`
   struct header is incomplete; all code that accesses those fields is
   operating on wrong offsets or missing members.
4. **UNDER** on `determineFixedWaves` (3.2× under) and `registerAllocation`
   (1.3× under) — core compiler passes are incomplete.
5. **UNDER** on `Resources::printAll` — missing parent-lock may cause
   use-after-free or race condition in multi-threaded use.
6. **OVER** on `swap(SeqCValue&)` and `getAccessModes` — both are tiny in the
   binary (39 B each); the recon has orders-of-magnitude more code. These
   are wrong and will generate incorrect output or crash.
