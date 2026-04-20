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

5. **`isWaveformCmd = (cmd - 3) < 3u`**
   This flags opcodes 3, 4, 5 (MESSAGE, unknown-4, ERROR_MSG). Why are these
   called "waveform commands"? Or is the field name wrong and it means something
   like "isPseudoInstruction"?

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

18. **AssemblerInstr outputs vector usage**
    The second vector<Immediate> at +0x38 is confirmed structurally but which
    instructions populate it and what semantics it carries is not yet clear.

19. **Node +0x18..+0x27 reserved fields**
    Two uint64_t fields zeroed in constructor. The destructor calls __release_weak
    on +0x20, suggesting +0x18/+0x20 may be a weak_ptr<something> (not Node).

20. **AsmRegister valid vs value semantics**
    Is valid ever true when value == -1? Is valid ever false when value >= 0?
    Need to check more call sites to understand the relationship.

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

29. **parseStringToAsmList hardcoded device type 2**
    Calls getDeviceConstants(AwgDeviceType(2)) — always device type 2 regardless
    of actual target. Is this a Hirzel default? Or does the serialized format
    only support one device type?

30. **Two TLS counters: TLS+0x40 (Asm::nextID) vs TLS+0x44 (Node::node_id_counter)**
    Both live in .tbss. Confirmed: +0x40 = Asm sequenceId counter, +0x44 = Node
    nodeId counter. They are adjacent but independent.

## NEW Phase 3c Questions

35. ~~**`operator==` epsilon value at 0x956350**~~ **RESOLVED (Phase 3c/3e)**
    Confirmed: **1e-12** (read directly from binary at 0x956350). Updated in signal.cpp.

36. **RawWaveData hierarchy**
    `getRawData()` creates instances of RawWavePlaceHolder (vtable 0xb077c8, 0x28 bytes),
    RawWaveCervino (vtable 0xb07868, 0x20 bytes with vector<uint16_t>), and
    RawWaveHirzel16 (0x20 bytes, ctor at 0x297140). Full layouts and virtual methods
    not yet reconstructed.

37. ~~**`checkAllocation()` does NOT clear `reserveOnly_`**~~ **CLOSED (Phase 3c)**
    Behavioral observation, not an open question. `checkAllocation()` is idempotent
    by design — the size check (`totalSamples > samples_.size()`) means subsequent
    calls are no-ops once vectors are materialized. The redundant calls in
    `append(Signal&)` are defensive/compiler artifact.

38. **Constructor #2 marker bit distribution logic**
    `Signal(size_t, double, uint8_t, uint16_t)` iterates `channels + (channels>1?1:0)`
    times, ORing marker into `markerBits_[i % size]`. The extra iteration when
    channels>1 means the first slot gets an extra OR. Why?

31. **RegisterBank field semantics**
    The four RegisterBank sub-structs (waveformReg, commandTableReg, sequencerReg,
    auxReg) each have base/stride/width/depth. The field names are inferred from
    cross-device value patterns, not from debug info. The "stride" field in
    particular varies wildly (0x200 to 0x100000) — it may be an address mask or
    range rather than a linear stride.

32. **DeviceConstants::Register anonymous enum naming**
    `{unnamed type#7}` = 0x44 and `{unnamed type#8}` = 0x45 are used in
    `unsyncCervino()` as sequencer register addresses. The numbering (#7, #8)
    suggests there are at least 6 other anonymous enums (#1–#6) in the Register
    type, but we haven't identified them yet. They may be used in other methods
    we haven't fully disassembled (syncCervino, etc.).

33. ~~**Device type 256 identity**~~ **RESOLVED (Phase 3d)**
    Device type 256 is **VHFLI** (codename "maloja"). Display name "VHFLI".
    Shares register config with SHFLI (type 64). Uses Cervino implementation.

34. ~~**WaveformFront.field70 = DeviceConstants.sequencerReg.width (+0x40)**~~ **CLOSED (Phase 3a)**
    Confirmed and renamed to `seqRegWidth` in waveform_front.hpp. Represents the
    number of sequencer registers available for waveform addressing.
