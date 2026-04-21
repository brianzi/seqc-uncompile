# Unknowns and Open Questions

## Structural

1. ~~**AssemblerInstr +0x50..+0x80 (0x30 bytes)**~~ **RESOLVED (Phase 2)**
   +0x38 = vector<Immediate> outputs, +0x50 = string label, +0x68 = string comment.
   Register order corrected: reg2(dest)/reg0(src1)/reg1(src2).

2. ~~**Node +0x04..+0x28 gap**~~ **RESOLVED (Phase 2)**
   +0x10 = id (TLS counter), +0x14 = deviceIndex, +0x18..+0x27 = two zeroed
   uint64_t fields (reserved / enable_shared_from_this padding).

3. ~~**Node fields beyond +0x104**~~ **RESOLVED (Phase 2d)**
   +0x108 = loopBodyRunsAtLeastOnce (bool), +0x109 = branchMaySkipAllBodies (bool),
   +0x10A = padding, +0x10C = trig (int). All field names confirmed from toJson JSON keys.

4. ~~**WaveformFront full layout**~~ **RESOLVED (Phase 2e, CORRECTED Phase 3c)**
   Full 0xF8-byte layout confirmed. Signal at +0x80 is 0x58 bytes (not 48 as
   originally thought). Fields at +0xB0/+0xC8/+0xD0 are Signal's internal
   markerBits_/channels_/length_ — not separate Waveform fields.

## Behavioral

5. ~~**`isWaveformCmd = (cmd - 3) < 3u`**~~ **RESOLVED (Phase 8e)**
   Flags Assembler::Command enum values 3, 4, 5 — three waveform-related
   commands. In serialize(), value 5 gets " #disableOpt" suffix. The field
   name is correct — these ARE waveform commands.

6. ~~**`nodeExtraRef_` at AsmCommands+0x54**~~ **RESOLVED (Phase 3e)**
   This is `numChannelGroups_` — copied from `AWGCompilerConfig+0x1c` (values 1, 2, or 4).
   Passed as 3rd arg to `Node::Node(NodeType, int, int)` where it becomes the initial
   capacity for `vector<optional<string>>` at Node+0x28 (one slot per channel group).
   Renamed from `nodeExtraRef_` to `numChannelGroups_`.

7. ~~**Thread-local counter reset**~~ **RESOLVED (Phase 3e)**
   Both counters ARE reset to zero inside `Compiler::compile()`:
   - TLS+0x40 (Asm nextID): unconditionally reset at start of compile (0x11f19f)
   - TLS+0x44 (Node idCounter): conditionally reset later (0x1209c6), when byte [rbx+0x28]==1
   - Both reset together at 0x1209b9 under the same condition (likely second-pass reset)
   Counters restart from 0 each compilation — NOT monotonically increasing.

8. **`syncCervino` and `unsyncCervino` complexity**
   Both are ~1000 lines of assembly each. They emit multi-instruction sequences
   for synchronization. Full reconstruction deferred — these are the most complex
   methods in the class.

9. **`addi32` exact behavior**
   The full 32-bit immediate variant has complex multi-instruction logic that
   differs from the standard `alui` path. Needs dedicated disassembly study.

10. **`smap` implementation**
    The disassembly suggests it delegates through ADDI, but the exact mapping
    semantics are unclear.

11. **`lcnt` and `luser`/`suser` opcodes**
    These may use LD/ST or may have dedicated opcodes. Current reconstruction
    assumes they reuse LD/ST.

## Data

12. ~~**PlayConfig static shift/mask constants**~~ **RESOLVED (Phase 2)**
    All shift/mask constants extracted. See play_config.hpp.

13. ~~**ErrorMessages format strings**~~ **RESOLVED (Phase 3b)**
    305 format strings extracted from global initializer at 0xd5de0. Stored in
    std::map<int,string> at BSS 0xb84c38. Keys: 1–255 (compiler, gaps at 47/53),
    16384–16389 (status), 32768–32800 (API), 36864–36877 (firmware).
    boost::format syntax (%1%, %2%, etc.). Full enum in error_messages.hpp.

14. ~~**Assembler::commandToString() mapping**~~ **RESOLVED (Phase 2a)**
    Full 43-entry cmdMap dumped. See notes/opcode_map.md.

## NEW Phase 2d Questions

23. **Node field name "deviceIndex" at +0x40 vs "asmId" at +0x14**
    Confusingly, the JSON key "deviceIndex" maps to the field at +0x40 (previously
    called waveformIndex — an index into wavesPerDev), while "asmId" at +0x14 is
    what toString() prints as "asm id". The naming suggests +0x40 selects which
    device's waveform to use, not a waveform slot index.

24. **Node field "play" at +0xA0 (vector<weak_ptr<Node>>)**
    JSON key is "play". This was previously called weakRefs. The name "play"
    suggests these are references to Play nodes that this node depends on or is
    associated with. Exact population mechanism still unknown.

25. **Node field "length" at +0x90 (int)**
    JSON key is "length", not "regVal" as previously assumed. This likely stores
    a waveform/segment length value rather than a generic register value.

26. **Node::toJson idMap uses asmId (+0x14), not nodeId (+0x10)**
    The remapping `idMap.at(asmId)` for the "nodeId" JSON key suggests the external
    caller maps by asmId, not by the internal nodeId. This is counter-intuitive —
    needs investigation of the serialization call site.

## NEW Phase 2 Questions

15. ~~**Opcode 0x60000007 (ALU_REG7)**~~ **RESOLVED (Phase 2a)**
    This is XORR — bitwise XOR register-register. Confirmed from cmdMap: "xorr".

16. ~~**Opcode 0xF1000000 (UNK_F1)**~~ **RESOLVED (Phase 2a)**
    This is WWVF — "write waveform". Confirmed from cmdMap: "wwvf".

17. ~~**Opcode 0xFB000000 (UNK_FB)**~~ **RESOLVED (Phase 2a)**
    This is WVFEI — "waveform end interleaved". Confirmed from cmdMap: "wvfei".

18. **AssemblerInstr outputs vector usage** *(NARROWED Phase 8e)*
    The second vector<Immediate> at +0x38 is confirmed structurally. Population
    sites are inlined within Assembler methods (not visible as direct push_back
    calls). Copy-ctor at 0x122e61, move-assign at 0x125eac. Needs deeper
    disassembly of expression evaluation to find actual population sites.

19. ~~**Node +0x18..+0x27 reserved fields**~~ **RESOLVED (Phase 8e)**
    +0x08/+0x10 and +0x18/+0x20 are two `weak_ptr`s. The dtor calls
    `__release_weak` on the control block pointers at +0x08 and +0x20.
    These are part of `enable_shared_from_this` (first weak_ptr at +0x00/+0x08)
    plus a second weak_ptr at +0x18/+0x20 (purpose TBD — possibly a
    back-pointer not exposed via JSON).

20. ~~**AsmRegister valid vs value semantics**~~ **RESOLVED (Phase 8e)**
    From AsmRegister::AsmRegister(int) at 0x28eb40:
    - `value = (val > 0) ? val : 0` (cmovg: clamp negative to 0)
    - `valid = (val >= 0)` (setns: true if non-negative)
    So AsmRegister(-1) → {value=0, valid=false} (invalid sentinel),
    AsmRegister(0) → {value=0, valid=true} (register 0),
    AsmRegister(N>0) → {value=N, valid=true}.

21. **PlayConfig::now field excluded from operator!=**
    The `now` field is deliberately not compared in operator!=. This suggests
    it is a transient/one-shot flag, not part of the configuration identity.

22. **PlayConfig::hold comparison is rate-gated**
    hold is only compared when rate > 0 in operator!=. This implies hold
    behavior is undefined/irrelevant for default-rate or stopped playback.

## NEW Phase 2b Questions

27. **serialize() opcode==4 skip**
    In AsmList::serialize(), entries with opcode==4 (NOP) are skipped in the
    idMap building pass. Why NOP specifically? Is this a "dead code elimination"
    step during serialization?

28. **serialize() " #disableOpt" suffix**
    Appended when isWaveformCmd && opcode==5. What is opcode 5 (ERROR_MSG)?
    The suffix suggests this marks instructions that should not be optimized by
    downstream passes.

29. ~~**parseStringToAsmList hardcoded device type 2**~~ **RESOLVED (Phase 8e)**
    Confirmed at 0x266188: unconditionally passes AwgDeviceType(2) = HDAWG to
    getDeviceConstants(). This is a convenience/test function — doesn't support
    arbitrary device types.

30. **Two TLS counters: TLS+0x40 (Asm::nextID) vs TLS+0x44 (Node::node_id_counter)**
    Both live in .tbss. Confirmed: +0x40 = Asm sequenceId counter, +0x44 = Node
    nodeId counter. They are adjacent but independent.

## NEW Phase 3c Questions

35. ~~**`operator==` epsilon value at 0x956350**~~ **RESOLVED (Phase 3c/3e)**
    Confirmed: **1e-12** (read directly from binary at 0x956350). Updated in signal.cpp.

36. ~~**RawWaveData hierarchy**~~ **RESOLVED (Phase 8d)**
    Fully reconstructed: abstract RawWave base + RawWavePlaceHolder (0x28),
    RawWaveHirzel16 (0x20), RawWaveCervino (0x20). Three conversion paths
    in Hirzel16 ctor based on SSE2 marker scan: double2awg16/double2awg1m/double2awg.
    See rawwave.hpp/rawwave.cpp.

37. ~~**`checkAllocation()` does NOT clear `reserveOnly_`**~~ **CLOSED (Phase 3c)**
    Behavioral observation, not an open question. `checkAllocation()` is idempotent
    by design — the size check (`totalSamples > samples_.size()`) means subsequent
    calls are no-ops once vectors are materialized. The redundant calls in
    `append(Signal&)` are defensive/compiler artifact.

38. **Constructor #2 marker bit distribution logic**
    `Signal(size_t, double, uint8_t, uint16_t)` iterates `channels + (channels>1?1:0)`
    times, ORing marker into `markerBits_[i % size]`. The extra iteration when
    channels>1 means the first slot gets an extra OR. Why?

31. ~~**RegisterBank field semantics**~~ **RESOLVED (Phase 7e)**
    The four "RegisterBank" sub-structs were a misinterpretation. Cross-referencing
    all consumer sites (Prefetch, WavetableIR, AWGAssemblerImpl, Cache, WaveformFront)
    revealed the 16-byte groups are unrelated scalar fields. RegisterBank struct removed;
    fields renamed to semantic names: waveformRegBase, waveformMemorySize, sampleLength,
    waveformAlignment, cachePageCount, maxBlocks, registerDepth, memoryDepth,
    sequencerRegBase, waveformGranularity, waveformPageSize, bitsPerSample, etc.

32. **DeviceConstants::Register anonymous enum naming**
    `{unnamed type#7}` = 0x44 and `{unnamed type#8}` = 0x45 are used in
    `unsyncCervino()` as sequencer register addresses. The numbering (#7, #8)
    suggests there are at least 6 other anonymous enums (#1–#6) in the Register
    type, but we haven't identified them yet. They may be used in other methods
    we haven't fully disassembled (syncCervino, etc.).

33. ~~**Device type 256 identity**~~ **RESOLVED (Phase 3d)**
    Device type 256 is **VHFLI** (codename "maloja"). Display name "VHFLI".
    Shares register config with SHFLI (type 64). Uses Cervino implementation.

34. ~~**WaveformFront.field70 = DeviceConstants.waveformGranularity (+0x40)**~~ **CLOSED (Phase 3a, updated Phase 7e)**
    Confirmed and renamed to `seqRegWidth` in waveform_front.hpp. Represents the
    number of sequencer registers available for waveform addressing.

## NEW Phase 4 Questions

39. **AWGAssemblerImpl+0x20 string purpose**
    The second string at offset 0x20 is zeroed in ctor but its purpose is unknown.
    filename_ is at +0x08, asmSource_ at +0x38. This middle string may be an
    include path or preprocessed source variant.

40. **AWGAssemblerImpl+0x120 stream object**
    Dtor checks if ptr at +0x120 equals &this+0x100 — if not, calls virtual dtor.
    This suggests an embedded stringstream or similar object with optional external
    allocation. Exact type unknown.

41. **AWGAssemblerImpl+0x148..0x160 hash table**
    A hash table (ptr + capacity + linked list head + count) is freed in dtor.
    Likely a parser symbol table or macro definition map. Nodes are 0x28 bytes
    with a string at +0x10.

42. ~~**AsmExpression full layout gaps**~~
    **CLOSED (Phase 10c):** Full 0xa8-byte layout determined from dtor @0x28b1f0.
    +0x04 = padding, +0x58 = zeroed spacer, +0x78 = hasLabel bool guard,
    +0x98 = hasComment bool guard. No unmapped fields remain.

43. ~~**AsmParserContext interface**~~
    **CLOSED (Phase 9b):** Full ~0x80-byte layout reconstructed. 22 accessor
    methods + Label inner type + addNode/addCommand/asmerror free functions.
    See asm_parser_context.hpp.

44. ~~**ElfWriter interface**~~
    **CLOSED (Phase 9a):** 0x78-byte layout (ELFIO::elfio base + memoryOffset_).
    8 methods reconstructed. Machine type passed from DeviceConstants, not
    always ET_EXEC. See elf_writer.hpp.

45. **assembleAsmList register ordering**
    The code adds reg0, reg1, reg2 as children but the ordering may not match
    the opcode encoding expectations (opcode3 expects dest in child[0]).
    Need to verify against actual AsmCommands output.

## NEW Phase 6 Questions

46. ~~**AsmOptimize constructor not reconstructed**~~ **RESOLVED (Phase 8e)**
    No out-of-line constructor — always inlined at callsite in Compiler::compile().
    Inline construction at 0x120707: takes CompilerMessageCollection& (for error/
    warning callbacks), numRegisters from DeviceConstants+0x28, and a device info
    pointer. Constructs two std::function wrappers around errorMessage (0x12b720)
    and infoMessage (0x12b9f0).

47. **removeUnusedRegs inner scan loop approximate**
    The core logic of removeUnusedRegs (~400 asm lines) has placeholder
    sections. The interaction between getNextActionForReg, simplifyAssign,
    and the write-only register tracking is not fully captured.

48. **registerAllocation conflict detection details**
    The exact conflict detection implementation (how live range overlaps are
    computed via set intersection) is approximate. The binary uses
    std::set<unsigned long> but the encoding of register pairs into the set
    key is not confirmed.

49. **splitConstRegisters splitting heuristic**
    The threshold for "long" live ranges that triggers splitting is not
    determined. The inner loop logic (~1800 bytes) is approximate.

50. **mergeRegisterZeroing: ADDR not XORR**
    The pass checks cmd == 0x50000000 (ADDR), not XORR as initially expected.
    The zeroing pattern is `ADDI r,0` + `ADDR r,r,r` rather than the
    expected `ADDI r,0` + `XORR r,r,r`. This may indicate XORR is not used
    for register zeroing in this ISA, or the method name is misleading.

51. ~~**CancelCallback interface**~~ **RESOLVED (Phase 8c)**
    Pure virtual interface with `isCancelled()` method. No vtable/typeinfo in
    binary — implementations live in Python binding layer. Reconstructed in
    callbacks.hpp. ProgressCallback also reconstructed (8-byte vtable-only class,
    empty setProgress default).

52. **isRead/isWritten: reg2 not checked**
    Neither isRead nor isWritten checks reg2 (+0x38). This is unexpected
    since reg2 is the destination register in AssemblerInstr. The query
    helpers use dest (+0x28), reg0 (+0x20), and reg1 (+0x30) only. Possibly
    reg2 and dest are aliases (both at different offsets mapping to the same
    logical slot) or reg2 is handled differently.

53. **getNextActionForReg register slot mapping**
    The function checks dest (+0x28), reg1 (+0x30), and reg2 (+0x38) —
    different from isRead/isWritten which check reg0/dest/reg1. The
    inconsistency in which slots are checked across different helpers
    suggests the slot semantics may vary by instruction type.

## NEW Phase 7a Questions

54. **Compiler+0x18 reserved field**
    An 8-byte field zeroed in the constructor. Purpose unknown. Could be
    a pointer to a sub-object that was removed in this version.

55. **SeqcParserContext layout at Compiler+0x100**
    The parser context is ~0x38 bytes and contains at least a std::function
    for error callback. Methods include reset(), hadSyntaxError(),
    setErrorCallback(). Full layout not yet determined — shares interface
    with AsmParserContext (unknown #43) but may be a different class.

56. **FrontendLoweringContext and FrontendLoweringState types**
    Both are stack-local in FrontEndLoweringFacade::lower(). Context holds
    shared_ptrs (AsmCommands, CustomFunctions, WaveformGenerator,
    WavetableFront) + int. State has a vector<string>. Neither type has
    been fully reconstructed.

57. **Expression vs SeqCAstNode type relationship**
    parse() returns shared_ptr<Expression> (old AST), which is converted to
    SeqCAstNode via toSeqCAst(). SeqCAstNode is polymorphic (vtable dispatch
    in lower()). Expression has an EOperationType field at +0x00. Neither
    type's layout has been reconstructed.

58. **getNodeAccessList / getNodeToModeMap return types**
    Both return pointers into the CustomFunctions object (offsets +0x150
    and +0x128 respectively). The actual types (likely maps or sets) are
    not yet determined.

59. **Compiler::compile() inner pipeline details**
    The 13KB compile() function's inner steps (linearize nodes, build
    assembly preamble, construct output vector) are captured at high level
    but the detailed logic of steps 10-11 and 19 is approximate.

60. ~~**AWGCompilerConfig debug flags field**~~ **RESOLVED (Phase 8a)**
    Located at +0x90, uint32_t. Bits: 0x02=print old AST, 0x04=print SeqC AST,
    0x08=print tree/assembly. Added to awg_compiler_config.hpp as `debugFlags`.

## NEW Phase 7b Questions

61. **Cache::memoryWrite overlap removal**
    The binary performs manual memmove-style shared_ptr shifting (adjusting
    refcounts inline) to remove overlapping entries. The reconstructed version
    uses semantic erase/insert which is functionally equivalent but doesn't
    match the binary's optimization.

62. **Cache::getBestPosition nameMap semantics**
    The nameMap maps waveform names to bool. When true, the pointer is skipped
    during gap calculation (it will be freed/reused). The exact contract
    between caller and getBestPosition is not fully documented — Prefetch
    likely builds this map from its wave management state.

63. **Cache::allocate 5-arg splitting heuristic**
    The first allocate overload computes `numAllocs` as `max(numSamples/freePages+1,
    numSamples/(size/2))` then allocates `numSamples/numAllocs` as Aligned.
    The exact intent (partial waveform loading? double-buffering?) is unclear.

64. **kCacheFormat at 0x95e8f8**
    Static SSO string containing "3". Purpose unknown — likely a version
    identifier for cache serialization format. Not referenced by any
    Cache method; may be used by CachedParser.

65. **unusedCacheLine at 0x95deac**
    Global constant = 0xFFFFFFFF. Sentinel for unallocated cache lines.
    Not directly used in Cache methods — likely used by Prefetch callers.

## NEW Phase 7c Questions

66. ~~**NodeType value discrepancies**~~ **RESOLVED (Phase 8e)**
    Full NodeType enum from type2str() at 0x269970:
    0x01=load, 0x02=play, 0x04=branch, 0x08=loop, 0x10=setvar, 0x20=rate,
    0x40=lock, 0x80=unlock, 0x100=sync_cervino, 0x200=table, 0x1000=setprecomp,
    0x2000=sync_hirzel, 0x4000=plainload, 0x8000=awg_ready.
    These are bitmask-style values. The earlier enum was incomplete.

67. ~~**NodeType 0x80 identity**~~ **RESOLVED (Phase 8e)**
    0x80 = "unlock". See #66 above for full enum.

68. **UsageEntry layout**
    Used internally by Prefetch optimization passes (optimize, allocate).
    Appears to be a small struct tracking per-node resource usage but
    exact field layout not confirmed from disassembly.

69. **Prefetch::minIndexedSize usage**
    Referenced in definePlaySize and related helpers. The semantic meaning
    (minimum waveform size for indexed playback?) and how it interacts
    with Cache alignment is not fully understood.

70. **PrefetcherNodeState full layout**
    Reconstructed as: 2 AsmRegisters (hirzel/cervino), counter, refTrack,
    pageSize, usedCache, shared_ptr<Cache::Pointer> cachePtr, useDA.
    Some fields may have slightly different offsets — the struct size is
    not confirmed from sizeof() evidence.

## NEW Phase 7d Questions

71. ~~**AWGCompilerConfig+0x19 = cacheType (uint8_t)**~~ **RESOLVED (Phase 8a)**
    Added to awg_compiler_config.hpp as `uint8_t cacheType` at +0x19.
    Values: 0=Normal, 1=Aligned.

72. ~~**DeviceConstants+0x18 as cache page count array**~~ **RESOLVED (Phase 7e)**
    The "commandTableReg RegisterBank" was a misinterpretation. DC+0x18 is
    `cachePageCount` and DC+0x1C is `maxBlocks` — two separate scalar uint32_t
    fields. clampToCache indexes them as `(&cachePageCount)[cacheType]` where
    cacheType is 0 (Normal) or 1 (Aligned). RegisterBank struct removed entirely.

73. ~~**Node+0x44 dual interpretation**~~ **RESOLVED (Phase 8e)**
    Node+0x44 IS `type` (NodeType). The "refCount" interpretation was WRONG.
    Confirmed from multiple Prefetch sites: prepareTree (0x1c8a4c), placeSingleCommand
    (0x1d79d2), needsNewCwvf (0x1dc66a) — all dispatch on NodeType bitmask values.
    The earlier confusion arose from non-Node objects being accessed at the same
    offset (+0x44) which happened to be a different field (e.g., DeviceConstants field).

74. ~~**Node+0x64 identity (indexed bool)**~~ **RESOLVED (Phase 8e)**
    Node+0x64 = PlayConfig.now (bool at PlayConfig+0x1C). PlayConfig is at Node+0x48
    (0x20 bytes), so +0x48+0x1C = +0x64. Confirmed: movzbl at 0x1de077 reads it as
    byte. Similarly +0x65 = PlayConfig.hold, +0x4C = PlayConfig.rate.

75. **placeSingleCommand cervino_indexed_nonsplit path**
    The inner path at ~0x1db562 for cervino non-split indexed playback
    is still approximate. This affects indexed waveform emission on
    older devices.

76. **print output goes to std::cout, not logFunc_**
    Despite having a logFunc_ callback, the print method writes to
    std::cout directly. This may be intentional (debug-only method)
    or may indicate logFunc_ is used elsewhere.

77. **Immediate variant destructor vtable at 0xb04da0**
    Used in wvfImpl/wvfs/insertPlay for destroying Immediate temporaries.
    The dispatch table is indexed by Immediate::index_ (discriminant).
    For int type (index=1), it's a no-op. Other types may need
    string/shared_ptr cleanup.
