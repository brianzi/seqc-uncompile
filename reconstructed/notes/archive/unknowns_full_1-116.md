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

8. ~~**`syncCervino` and `unsyncCervino` complexity**~~ **CLOSED (Phase 10.5d)**
   Both fully reconstructed. syncCervino @0x275c50 (4.3KB): master/slave flag
   sync via registers 0x44/0x45. unsyncCervino @0x276d10 (1.2KB): 2 ST instructions.

9. ~~**`addi32` exact behavior**~~ **CLOSED (Phase 10.5c)**
   Confirmed: always 2-instruction sequence: ADDI(low12) + ADDIU(upper),
   both with isWaveformCmd=true.

10. **`smap` implementation** *(NARROWED Phase 10.5c)*
    Calls alui(ADDI, r1, reg0, arg), but ~0x1E6 bytes of conditional logic
    after the alui call remain unanalyzed. Marked APPROXIMATE in source.

11. ~~**`lcnt` and `luser`/`suser` opcodes**~~ **CLOSED (Phase 10.5c)**
    `luser`/`suser` confirmed as direct delegates to `ld`/`st` (same opcode).
    `lcnt` remains a separate question but is low priority.

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

19. ~~**Node +0x18..+0x27 reserved fields**~~ **RESOLVED (Phase 8e / refined Phase 10.8a)**
    +0x08/+0x10 and +0x18/+0x20 are two `weak_ptr`s. The dtor calls
    `__release_weak` on the control block pointers at +0x08 and +0x20.
    The first (+0x00/+0x08) is part of `enable_shared_from_this`.
    The second (+0x18/+0x20) is `Node::loadRef` — a non-owning back-reference
    from a Play node to its associated Load node (set by Prefetch::linkLoad,
    consumed by Prefetch::allocate's Play case via `loadRef.lock()` to
    perform `Cache::play(loadCachePtr, pointerState)`). Modeled as
    `std::weak_ptr<Node> loadRef` in node.hpp; see libcpp_abi.md for the
    16-byte layout match.

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

56. **FrontendLoweringContext and FrontendLoweringState types** — RESOLVED (Phase 11c)
    Both reconstructed in frontend_lowering.hpp.  Context is 0x50 bytes
    (raw CompilerMessageCollection* + 4 shared_ptrs + int).  State is
    0x30 bytes (shared_ptr<void> + pad + vector<string>).  Remaining
    unknowns: State result shared_ptr type (#98), lower() return type (#97).

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

70. ~~**PrefetcherNodeState full layout**~~ — **RESOLVED (Phase 10.5e)**
    0x40 bytes total. Corrected init values: state=3 (unloaded), branchCount=1,
    pageSize=1. Field renamed: counter→state, usedCache→requiredSlots.
    Typed shared_ptr<Cache::Pointer> (was shared_ptr<void>).

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

78. **CachedParser::CacheEntry layout** (opened Phase 10.7b-11)
    The nested type `zhinst::CachedParser::CacheEntry` is referenced
    extensively by boost::serialization symbols (extended_type_info_typeid
    and iserializer specializations at b84c80..b84e30) but no constructor
    or method body is visible in `objdump -dC` output for it. Likely
    plain-old data with serialization-only access. Layout TBD; not
    needed for the public CachedParser interface (only internal map
    storage). Probable fields based on the access pattern in
    loadCacheIndex (mov rdx, [rcx+0x68] at 0x2aff80 — accumulating
    a per-entry size during iteration): includes at least one size_t
    `size` field at offset +0x60-ish from the map's __value_type pair.

79. **zhinst::Exception base class** (opened Phase 10.7b-11)
    Discovered as the true base of ZIAWGCompilerException (and
    ZIAWGOptimizerException, and likely several others). Total layout
    of zhinst::Exception not yet reconstructed but key facts:
    - Multiple inheritance: derives from `std::bad_exception` (primary)
      and `boost::exception` (secondary subobject at +0x8)
    - Has at least 4 ctors: default 0x2e5410, (string) 0x2e54b0,
      (boost::error_code) 0x2e55b0, (error_code, string) 0x2e5700,
      (GenericErrorDescription<error_code>) 0x2e5810
    - Public methods: what() @ 0x2e5870, code() @ 0x2e5890,
      description() @ 0x2e58b0
    - Stores message string at +0x48 (24 bytes libc++) — visible in
      ZIAWGCompilerException dtor at 0x2e9b34: `mov rsi, [rbx+0x48]`
    - Total derived-class size 0x60 bytes for ZIAWGCompilerException
    Should be reconstructed into its own header `exception.hpp`,
    after which all derived exception classes can be properly
    re-derived from it instead of from std::exception.
    **RESOLVED Phase 10.7d.** Full layout reconstructed in
    include/zhinst/exception.hpp + src/exception.cpp. Verified
    fields: vptrs at +0x00/+0x08, boost::exception data at
    +0x10..+0x2F, errorCode_ (24B) at +0x30, message_ (24B libc++
    SSO) at +0x48, total size 0x60. All 5 ctors, dtor, and 3
    observers (what/code/description) reconstructed with binary
    addresses noted. ZIAWGCompilerException + ZIAWGOptimizerException
    re-derived from Exception (both add zero data fields, only
    differ by vtable identity). Other 24 derived types documented
    in header for future on-demand addition.

80. **zhinst::GenericErrorDescription<T> template** (opened Phase 10.7b-11)
    Referenced by Exception::Exception(GenericErrorDescription<error_code>)
    @ 0x2e5810. Template structure carrying error_code + extra metadata
    (probably location info). Layout TBD. Used in compiler error paths
    for richer error contexts.
    **RESOLVED Phase 10.7d.** Layout is `{T (24B), std::string (24B)}`
    = 48 bytes total. Verified at the (GenericErrorDescription) ctor
    parameter access pattern: xmm[+0x00], q[+0x10] copy into errorCode_
    field; xmm[+0x18], q[+0x28] move into message_ field with source
    nulled out (moved-from sentinel). Reconstructed as a template
    struct in exception.hpp; not currently used by seqc src/ but
    available for the asException helper at 0x2ea190.

81. **Prefetch::placeSingleCommand case 1/case 2 source merger** (opened Phase 10.8b)
    The reconstructed source has all NodeType::Load and NodeType::Play
    code under a single `case 1:` label, while the binary's jump table
    at 0x95af74 dispatches them to distinct entry points (Load at
    0x1d79f8, Play at 0x1d7d49). The body content is correctly
    reverse-engineered, but the case-label structure is misleading.
    A future structural pass should split this. File-header comment
    in src/prefetch_placesingle.cpp documents the address-range
    attribution. Same systemic mislabel pattern resolved in 10.8a
    for `Prefetch::allocate`, `Prefetch::prepareTree`, `Prefetch::print`.

82. **Prefetch::findPlaceholder return type** (opened Phase 10.8b)
    Currently declared returning `shared_ptr<Node>` (likely a legacy
    source-level decision to keep the Node identity around for the
    `AsmList::insert(node, ...)` overload at asm_list.hpp:175).
    The binary returns `AsmList::Asm*` (an iterator into the AsmList
    vector). Changing the declaration to `AsmList::iterator` would
    enable the `placeholder->wavetableFront` line in placeSingleCommand
    (currently disabled with a comment) and align with binary semantics.
    Functionally a no-op for compilation correctness; affects only
    assembler diagnostic line-number tracking.

83. **WaveformIR::crossesCacheLine_ semantics confirmation**
    (resolved Phase 10.8c, recorded for future verification)
    Renamed from `irBool1` based on writer enumeration:
      - 0x2a9e47: set true in WavetableIR::allocateWaveforms's filler branch
      - 0x2aa88b: copy bit 8 of allocator block flags
      - 0x2ad023: copy bit 8 of allocator block flags (FIFO variant)
      - 0x2ad0ac: cleared to 0 right after writing addressValue (+0x4C),
                  i.e. when a normal waveform receives a final placement
                  that does NOT cross a cache line.
    Open question: confirm by tracing the consumer side. The field is
    only read inside WavetableIR's own allocation lambdas (no other
    file references +0xDA). If a future analysis finds a downstream
    reader that uses this as e.g. a "needs alignment fixup" flag, the
    name should be revisited.

84. **WaveformIR third constructor `(string, File::Type, DC&)`**
    (resolved Phase 10.8c, recorded for follow-up)
    Discovered via the `allocate_shared<WaveformIR>` dispatcher at
    0x2aa170 which inlines a ctor body that initializes Waveform::name
    (+0), waveformType (+0x18) and deviceConstants_ (+0x78), zeroes
    everything else, and sets waveIndex (+0x6C) to -1.
    Header now declares the ctor; no separate symbol exists in the
    binary because the dispatcher inlined it. If a future translation
    unit wants to actually call this ctor outside `allocate_shared`,
    we'll need to also write the inline definition (the body is
    documented in the dispatcher trace at 0x2aa1a1-0x2aa20f).

85. **prefetch.cpp moveLoadsToFront field labelling regression**
    (resolved Phase 10.8c, recorded as audit note)
    Source comment at line 742 claimed `+0xDA / irBool1`; binary at
    0x1ccb7f reads `cmp [rax+0xd8], 0x1` → `markedForLoad`. Surfaced
    only because the irBool1 rename triggered a cross-file grep.
    Action item: when renaming any field, ALWAYS grep for the byte
    offset (here `0xd8` and `0xda`) in addition to the name — comments
    that reference offsets are independent of the typed field name and
    can drift independently.

86. **AWGAssemblerImpl::opcodes_ element type was uint64_t (WRONG)**
    (resolved Phase 10.8d)
    Header `awg_assembler_impl.hpp` declared `std::vector<uint64_t>
    opcodes_` at +0x50. Disasm at 0x2885cf-0x2885da proves it must be
    `std::vector<uint32_t>`: writeToFile() loads `lea rsi,[r14+0x50]`
    and calls `ElfWriter::addCode(std::vector<unsigned int> const&)`
    directly, with no conversion. Corrected the field type and
    propagated through `getOpcode()` returns in both
    `AWGAssemblerImpl` and the public `AWGAssembler` wrapper. Note
    that `sizeof(std::vector<T>)` is 0x18 on libc++ regardless of T,
    so all surrounding offsets in the struct were unaffected — a
    subtle silent-corruption hazard if any future caller had read the
    return value as `uint64_t*`.

    Lesson: type errors in container element types do NOT show up as
    layout-shift errors. They are only catchable by following calls
    to functions whose signatures are visible in the symbol table
    (because demangled C++ function symbols carry the full template
    arguments, while raw memory accesses do not).

87. **prepareTree traversal local was std::deque (WRONG, should be std::stack)**
    (resolved Phase 10.8d)
    `Prefetch::prepareTree` declared its traversal local as
    `std::deque<shared_ptr<Node>>`. The cross-TU call to
    `removeBranches` at 0x1c8cd7 carries a demangled symbol
    `removeBranches(shared_ptr<Node>, std::stack<shared_ptr<Node>,
    std::deque<shared_ptr<Node>>>&)`. The `rdx` arg at the call site
    is the traversal local (loaded from `[rbp-0x60]`), so the local
    MUST be a `std::stack`. Fixed by changing the declaration and
    translating push_back/pop_back/back → push/pop/top across all 5
    in-function operations.

    Untouched: `Prefetch::countBranches` and `Prefetch::definePlaySize`
    have similar traversal locals but pass them only to local
    operations, so the disasm cannot disambiguate stack-vs-deque for
    those. Left as `std::deque` (status quo) until cross-TU evidence
    arises.

    Lesson: container choice for purely-local variables is invisible
    in raw disassembly when only push/pop operations are used. The
    only ground-truth signal is when the local is passed to a function
    whose mangled symbol pins the type.

88. **Hallucinated TODO: AWGAssemblerImpl::assembleAsmList "addLabel" call**
    (resolved Phase 10.8d)
    A reconstruction comment at `awg_assembler_impl_pipeline.cpp:323`
    suggested the binary calls `parserCtx_.addLabel(labelStr)` when
    handling LABEL pseudo-instructions. Disasm at 0x287e33 shows the
    binary instead constructs a *local* `AsmParserContext::Label{
    labelCounter, labelStr}` whose only effect is to hold a copy of
    the string until it's moved into `AsmExpression::labelName`. There
    is no addLabel/hasLabel-set registration here — the label set
    population happens via a separate code path (the `addLabel(string)`
    function at 0x28ea60 is called from elsewhere, not from
    assembleAsmList).

    The labelName/hasLabel/labelIndex assignment block immediately
    following the Label ctor (lines 327-338) was already correct: r14
    is the make_shared control block, so [r14+0x70]/+0x78/+0x90] map
    to AsmExpression+0x58/+0x60/+0x78 = labelIndex/labelName/hasLabel.

    Lesson: short-lived "holder" temporaries that exist only to feed a
    move-into-member can mask the actual data flow during
    reconstruction. Always trace the destination of every Label-like
    object's fields before assuming a registration call.

89. **Stale TODOs that turned out to reference already-defined fields**
    (Phase 10.8d audit findings)
    Three "TBD field" markers were resolved by simply consulting the
    existing header (no struct changes needed):
    * `prefetch_helpers.cpp:325` — `branchesModified` was a hallucinated
      name for `Node::branchMaySkipAllBodies` at +0x109 (already in
      `node.hpp:297`).
    * `prefetch_prepare.cpp:660` — `PlayConfig::unknown_1e` was a
      placeholder for `PlayConfig::dummy` at +0x1E (already in
      `play_config.hpp:40`). Offset arithmetic: 0x66 = 0x48
      (Node::config base) + 0x1E.
    * `prefetch_helpers.cpp:539` — slotIndex/data[]/hasValue TODO was
      stale; the existing `wavesPerDev[deviceIndex]` reads exactly the
      bytes the binary reads at 0x1d6277 (stride 0x20 = sizeof(
      optional<string>), has_value flag at +0x18, SSO/long string at
      +0x00..+0x17).

    Action item: before adding ANY new struct field, grep the relevant
    header for the documented byte offset and the rough field semantics
    — at least 30% of "missing field" TODOs in this project have turned
    out to be naming-convention drift between the disasm-comment author
    and the header maintainer.

90. **zhinst::Exception(error_code) message prefix string** (opened Phase 10.7d)
    The 1-arg error_code ctor at 0x2e55b0 calls
    `boost::system::error_code::to_string()` then `string::insert(0, ".rodata 0x90c6c6", 0x1e)`
    with a 30-byte prefix string. Only the trailing portion (likely
    "; ") was inferred in this pass — the full 30 bytes at
    .rodata 0x90c6c6 were NOT extracted. The reconstructed source uses
    an empty-string placeholder which is acceptable because no seqc
    compiler call site uses this ctor (only the (string) and
    (GenericErrorDescription) ctors are reachable from src/). A future
    pass could extract the literal from .rodata if any non-seqc API
    path becomes relevant.

91. **zhinst::Exception default ctor's make_error_code(0x8000) sentinel** (opened Phase 10.7d)
    The 0-arg ctor at 0x2e5410 stores `make_error_code(0x8000)` in
    errorCode_. The constant 0x8000 is presumably a ZIResult_enum
    value (per the called function name `make_error_code(ZIResult_enum)`
    at 0x2e4550) representing a generic "uncategorized exception"
    code. The exact enum identifier is not yet mapped — the reconstructed
    default ctor leaves errorCode_ default-initialized (value=0,
    category=null) which is observable only via `code()` / `description()`
    on a default-constructed Exception, neither of which any seqc
    compiler call site exercises. Resolve when ZIResult_enum is fully
    enumerated.

92. **24 declared-but-not-reconstructed Exception derivatives** (opened Phase 10.7d)
    The binary defines ZIAPIException, ZIIOException, ZIDeviceException,
    ZISocketException, ZIOverflowException, ZIUnderrunException,
    ZITimeoutException, ZIReadOnlyException, ZIWriteOnlyException,
    ZINotFoundException, ZIInvalidKeywordException, ZITypeMismatchException,
    ZIOutOfRangeException, ZIInterruptException, ZIInternalException,
    ZIDeviceNotVisibleException, ZIDeviceNotFoundException,
    ZIDeviceInUseException, ZIDeviceInterfaceException,
    ZIDeviceConnectionTimeoutException,
    ZIDeviceDifferentInterfaceException, ZIDeviceFWException,
    ZIVersionException, ZIIllegalPathException — none of which appear
    on any reconstructed seqc compiler code path. Documented in the
    header banner of exception.hpp but not declared. Add per-class
    declarations on demand if/when a future phase finds a
    constructed/thrown instance in the seqc compiler region of the
    binary. Most likely irrelevant: these are LabOne-API shared
    library exceptions that the embedded seqc compiler never raises.

93. **Expression pushChild ownership model** (Phase 11a)
    The binary's create* functions allocate a 0x20-byte control block
    (vptr @0x943e90, raw Expression* stored at +0x18) for each child
    shared_ptr.  The vptr points to __shared_ptr_pointer — indicating
    the shared_ptr takes ownership of the raw pointer.  Reconstruction
    uses a no-op deleter shared_ptr as a workaround since the memory
    ownership model across create* calls is not fully understood (the
    caller receives a raw Expression* which must eventually be wrapped
    in shared_ptr).  Clarifying the exact ownership semantics would
    require tracing the bison parser actions that call these functions.

94. **EDirection / EParamDirection naming collision** (Phase 11b)
    Two distinct direction-like enums exist in the reconstruction:
      - resources.hpp::EDirection — 2 values (Read=0, Write=1).
        Inferred without binary symbol evidence (resources.hpp:65
        explicitly notes "No EDirection symbolic constants exist in
        the binary's symbol table"). Used by 4 readXxx() Resources
        methods.
      - seqc_ast_node.hpp::EParamDirection — 3 values (eIN/eOUT/eINOUT).
        Has binary-confirmed values via str()@0x1c1730 with a jump
        table.  Stored at SeqCAstNode +0x10 and Expression's direction
        slot.
    These are semantically unrelated and only collided on a generic
    name.  The AST-side enum was renamed to EParamDirection to avoid
    rippling through 4 Resources callers.  TODO: if the binary's true
    symbol for either is recovered (e.g., from RTTI typeinfo strings or
    debug fragments), do a follow-up rename pass to use the canonical
    names.

95. **SeqCAstNode subclass method bodies** (Phase 11b — deferred)
    All 53 named SeqCAstNode subclasses are declared with full member
    layouts and virtual signatures, but their method implementations
    (clone, print, evaluate, per-node accessors) are stubbed with TODO
    markers and binary addresses noted in comments.  Per user-confirmed
    Phase 11b scope ("Layout + base + class declarations only"), full
    body reconstruction is deferred.  Recommended follow-up: a Phase
    11b-extended that walks each class's vtable and reconstructs
    method bodies one family at a time (trivial leaves first, then
    unaries, then SeqCOperator family, then containers).

96. **SeqCAstNode `type` field meaning** (Phase 11b)
    The base class has an `int type` at +0x0C (ctor arg #2).  Purpose
    is currently unclear — it does not appear to be EOperationType
    (different value range observed at construction sites).  May be
    a SeqC type-system tag (matching the resources.hpp VarType enum?)
    or a parser-level type discriminator.  Resolved in printSeqCAst's
    helper @0x1fa430 by literally printing `"  type: " + node.type` so
    it's at least surfaced to user output.  Determining the exact
    semantics requires correlating callers across the bison action
    layer with type-system code.

97. **FrontEndLoweringFacade::lower() return type** (Phase 11c)
    The current declaration in compiler.hpp says `void lower(...)` but
    the binary uses an sret pointer in rdi to return a 16-byte struct
    (a shared_ptr).  After the call in Compiler::compile(), the result
    is moved into `Compiler::ast_` at +0x28 (typed `shared_ptr<Node>`).
    The return comes from the virtual dispatch on SeqCAstNode (vtable
    slot 0), which receives `(sret, this, shared_ptr<Resources>,
    FrontendLoweringContext*, FrontendLoweringState*)`.  Fixing the
    signature requires: (a) determining the exact return type, (b)
    updating compiler.hpp and compiler.cpp, (c) understanding what
    `Node` actually is (currently only forward-declared).

98. **FrontendLoweringState::result shared_ptr type** (Phase 11c)
    The shared_ptr<void> at +0x00 is zeroed on construction and
    populated during the virtual lowering dispatch.  After lower()
    returns, it's copied to one half of the return struct.  The
    actual pointee type is unknown — possibly shared_ptr<Resources>
    (a new child scope) or shared_ptr<some-lowering-output>.

99. **EvalResults struct layout** (Phase 11c)
    Allocated as 0x80 bytes (make_shared allocation = 0x98 including
    0x18-byte control block).  Zero-initialized in constWaveform
    @0x22c9f0 with an explicit `BYTE [+0x30] = 0`.  Used as the
    return type of WaveformGenerator::eval().  Internal layout is
    completely unknown — needs analysis of eval() and its callers.

100. **EvalResultValue layout** (Phase 11d)
    Size: 0x38 (56 bytes). Element stride confirmed from vector relocate
    at 0x1729e0 (add r12,0x38). Contains a Value-like variant discriminator
    at +0x10 (same sar/xor/cmp 3 pattern as Value::which_) and libc++ SSO
    string data at +0x18. Fields at +0x00 (8B), +0x08 (4B), and +0x30 (8B)
    are unknown — could be shared_ptr parts, type tags, or line numbers.
    Needs cross-referencing with call sites that construct EvalResultValue.

101. **CustomFunctions field_168 purpose** (Phase 11d)
    At offset +0x168, there's a 40-byte region that looks like another
    unordered_map (destroyed in dtor @0x127c90). Key and value types unknown.

102. **MathCompiler internal layout** (Phase 11d)
    MathCompiler occupies 0x30 bytes at CustomFunctions +0xC8. Ctor @0x1c2250
    likely populates two unordered_maps (single-arg dispatch and multi-arg
    dispatch). Need to disassemble the ctor to determine exact field types.

103. **AccessMode enum values** (Phase 11d)
    toString(AccessMode) uses a static table at .rodata 0x9573c0. Need to
    read the table to determine exact enum values and their string names.
    Currently using placeholder Read=0, Write=1.

104. **NodeMapData subclass field layouts** (Phase 11d)
    VirtAddrNodeMapData and DirectAddrNodeMapData have unknown internal
    fields. DirectAddrNodeMapData likely contains a uint32_t address field
    (used by getNodeAddress @0x16ba10 via dynamic_cast fast path).
    VirtAddrNodeMapData likely contains a string path and possibly an
    int index (from hash() and compareEq() patterns).

105. **CustomFunctions::oscMaskCheckHirzel full logic** (Phase 11d)
    This function @0x15bab0 is complex — searches a features vector for
    "MF", inserts into usedFeatures_ set, then switches on device class
    and group index. The stub currently returns false; needs full
    disassembly to determine the switch cases and mask validation logic.

106. **CustomFunctions warningCallback_ layout** (Phase 11d)
    The std::function at +0x190 in the binary (libc++ ABI) is 48 bytes,
    but the gap from +0x190 to +0x1C8 is 56 bytes. Either there's 8 bytes
    of padding, or the function's inline storage extends further.
    Our libstdc++ std::function is 32 bytes, so the private field layout
    uses a combination of the real function + padding to bridge the gap.

107. **WaveformGenerator::field_20_ and field_48_ purpose** (Phase 12a)
    Both are float fields initialized to 1.0f. In libc++ unordered_map,
    the last 4 bytes are max_load_factor (default 1.0f). However, these
    fields sit at +0x20 and +0x48, which are exactly where the maps' own
    max_load_factor would be. It's possible these are NOT separate fields
    but rather the tail of the unordered_map's __hash_table layout.
    If so, the maps would be 0x24 bytes each (padded to 0x28), and
    field_20_/field_48_ would be artifacts. Needs verification by
    checking whether any code writes to +0x20/+0x48 independently of
    the map operations.

108. **WaveformGenerator::field_50_ purpose** (Phase 12a)
    A set<string> at +0x50. Purpose unknown — could be a set of used
    waveform names, feature flags, or cached keys. No code was analyzed
    that writes to this set.

109. **WaveformGenerator::field_78_ purpose** (Phase 12a)
    8 bytes at +0x78 between wavetableFront_ (+0x68) and
    warningCallback_ (+0x80). Zeroed in ctor; never written in
    the analyzed dtor path. Could be padding or an unused field.

110. **WaveformGenerator::field_B0_ purpose** (Phase 12a)
    A shared_ptr at +0xB0, initialized to null. The dtor calls
    __release_weak() on the control block at +0xB8, confirming it's
    a shared_ptr. Not set in the constructor — likely populated later
    by some setter method not yet identified.

111. **WaveformGenerator aliasMap_ exact contents** (Phase 12a)
    The unordered_map<string,string> at +0x28 maps deprecated function
    names to current names. The exact mapping pairs are embedded in the
    ctor's ~5KB of repetitive registration code (0x248200..0x249b90)
    but were not fully extracted. The call() method searches this map
    first and emits a warning via warningCallback_ when a match is found.

112. **WaveformGenerator::reverse multi-channel logic** (Phase 12a)
    The reverse() method is ~548 lines of disassembly (0x260f20..0x261750).
    The single-channel path is straightforward (std::reverse). The
    multi-channel path is more complex with block-based reversal.
    The current reconstruction is approximate — the exact interleaving
    logic for markers in multi-channel mode needs verification.

113. **CachedFile field ordering vs cacheFile args** (Phase 12b)
    The cacheFile() signature has args: (name, hash, sampleFormat, markers,
    markerBits, samples, markerBitsVec). The CachedFile struct has:
    found_(+0x00), samples_(+0x08), markers_(+0x20), markerBits_(+0x38).
    The mapping between cacheFile's 7 args and the 3 vectors stored in
    CachedFile is inferred but not fully verified from the cacheFile body.
    The "sampleFormat" and "markerBits" int args may control encoding format
    rather than being stored directly.

114. **CacheEntry boost::serialization::serialize template** (Phase 12b)
    The serialize() template is instantiated for text_iarchive and
    text_oarchive (confirmed by ~20 singleton instantiation symbols).
    The exact field order in the serialization is not reconstructed.
    The template declaration exists in cached_parser.hpp but has no body.

115. **CachedParser::cacheFile full body** (Phase 12b)
    The cacheFile() method at 0x2b05b0 is ~700 instructions. It constructs
    a CacheEntry, inserts into the map, writes binary data to a file named
    by the hash, and calls saveCacheIndex(). The exact file format (header,
    data layout) is not reconstructed.

116. **CachedParser::getCachedFile full body** (Phase 12b)
    getCachedFile() at 0x2b1900 is ~400 instructions. It does a map lookup,
    calls cacheFileOutdated, reads binary data from the cache file, and
    populates a CachedFile. The exact read logic is not reconstructed.
