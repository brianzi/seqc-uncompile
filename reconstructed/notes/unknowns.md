# Unknowns and Open Questions

## Structural

1. ~~**AssemblerInstr +0x50..+0x80 (0x30 bytes)**~~ **RESOLVED (Phase 2)**
   +0x38 = vector<Immediate> outputs, +0x50 = string label, +0x68 = string comment.
   Register order corrected: reg2(dest)/reg0(src1)/reg1(src2).

2. ~~**Node +0x04..+0x28 gap**~~ **RESOLVED (Phase 2)**
   +0x10 = id (TLS counter), +0x14 = deviceIndex, +0x18..+0x27 = two zeroed
   uint64_t fields (reserved / enable_shared_from_this padding).

3. ~~**Node fields beyond +0x104**~~ **RESOLVED (Phase 2)**
   +0x108 = boolField1, +0x109 = boolField2, +0x10A = padding, +0x10C = intField2.
   Total size confirmed 0x110.

4. **WaveformFront full layout**
   We only know ~6 fields at specific offsets. The struct has at least 0xCA bytes
   and likely much more. The name/string representation is accessed but the exact
   mechanism (operator*, getName(), etc.) is unknown.

## Behavioral

5. **`isWaveformCmd = (cmd - 3) < 3u`**
   This flags opcodes 3, 4, 5 (MESSAGE, unknown-4, ERROR_MSG). Why are these
   called "waveform commands"? Or is the field name wrong and it means something
   like "isPseudoInstruction"?

6. **`nodeExtraRef_` at AsmCommands+0x54**
   Passed as second argument to Node constructors. Purpose unclear — could be a
   scope/context ID, a line number, or a reference count.

7. **Thread-local counter reset**
   The TLS counter at offset 0x40 provides unique IDs but we don't know when/how
   it's reset between compilation runs. If it isn't, IDs grow monotonically across
   compilations.

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

13. **ErrorMessages format strings**
    The actual format strings for ErrorMessageT values 0, 5, 11 are in .rodata.
    Could be extracted by tracing the format() call addresses.

14. **Assembler::commandToString() mapping**
    Global cmdMap at 0xb84c20 — the full opcode-to-string table. Structure and
    content not yet fully dumped.

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
