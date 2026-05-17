# Magic Numbers & Missing Named Constants — Reference {#notes_magic_numbers_proposal}

This document catalogs fields with discriminator / enum / type-tag
or bitfield-flag semantics where raw integer literals appear instead
of named constants.  It provides the value tables, semantic
descriptions, and suggested enum definitions as reference material.
Where actionable refactoring is desired, the relevant items are
tracked in `TODO.md`.

---

## Category A: Fields needing NEW enum/constant definitions

These are places where no named enum or constant exists at all.

### A1. `Immediate::index_` — variant discriminator

| Value | Meaning |
|-------|---------|
| 0 | Address (unsigned) |
| 1 | Integer (signed int32) |
| 2 | String |
| 0xFFFFFFFF | Valueless / empty |

Proposal:
```cpp
enum class ImmediateKind : uint32_t {
    Address  = 0,
    Integer  = 1,
    String   = 2,
    Valueless = 0xFFFFFFFF
};
```

### A2. `AsmExpression::type` — expression node type

| Value | Meaning |
|-------|---------|
| 0 | Container / arglist |
| 1 | Register |
| 2 | Label / name |
| 3 | Integer / value |

Canonical enum (already defined in `asm_expression.hpp`):
```cpp
enum class AsmExprType : int { Container=0, Register=1, Label=2, Integer=3 };
```

### A3. `Variable::value.which_` — variant slot discriminator

| Value | Meaning |
|-------|---------|
| 0 | int |
| 1 | bool |
| 2 | double |
| 3 | string |

Proposal:
```cpp
enum class VariantSlot : int { Int=0, Bool=1, Double=2, String=3 };
```

### A4. `Variable::flags` — variable state flags

| Mask  | Name              | Meaning                                 |
|-------|-------------------|-----------------------------------------|
| 0x01  | `VarFlag_Written` | Variable has been written.              |
| 0x100 | `VarFlag_Frozen`  | Frozen (high byte, not bit 1).          |

Canonical constants already exist in `runtime/resources.hpp`:
```cpp
static constexpr int16_t VarFlag_Written = 0x01;
static constexpr int16_t VarFlag_Frozen  = 0x100;
```

\see `reconstructed/include/zhinst/runtime/resources.hpp`

### A5. `AsmOptimize::optFlags_` — optimization pass bitmask

| Mask | Meaning                                   |
|------|-------------------------------------------|
| 0x01 | `oneStepJumpElimination`                  |
| 0x02 | `removeUnusedLabels` + `mergeLabels`      |
| 0x04 | `deadCodeElimination`                     |
| 0x08 | `mergeRegisterZeroing`                    |
| 0x10 | `removeUnusedRegs` + `registerAllocation` |

Proposal:
```cpp
enum OptPassFlag : uint32_t {
    Opt_JumpElim     = 0x01,
    Opt_LabelCleanup = 0x02,
    Opt_DeadCode     = 0x04,
    Opt_MergeZero    = 0x08,
    Opt_RegAlloc     = 0x10,
};
```

### A6. `CompilerMessage::type` — message severity

| Value | Meaning |
|-------|---------|
| 0 | Error   |
| 1 | Warning |
| 2 | Info    |

Proposal:
```cpp
enum class MessageType : int { Error=0, Warning=1, Info=2 };
```

### A7. Device factory option bits

| Mask   | Meaning                          |
|--------|----------------------------------|
| 0x1C0  | Subtype selector mask (bits 8:6) |
| 0x040  | Subtype slot 1                   |
| 0x080  | Subtype slot 2                   |
| 0x0C0  | Subtype slot 3                   |
| 0x100  | Subtype slot 4                   |
| 0x020  | FF option                        |
| 0x2000 | RTR option                       |
| 0x4000 | PLUS option                      |
| 0x8000 | LRT option                       |

Proposal: named `constexpr unsigned long` constants for each.

### A8. Opcode encoding format categories (0–5)

| Value | Bit format | Instructions |
|-------|-----------|--------------|
| 0 | base unchanged | END, NOP |
| 1 | reg<<24 \| imm20 | ADDI, ADDIU, ANDI, ANDIU, ORI, ORIU, XNORI, XNORIU |
| 2 | reg<<24 \| 3×imm8 | PRF, WVF, WVFI, WTRIG |
| 3 | reg1<<24 \| reg2<<20 \| imm20 | ADDR..XORR, WVFS_H |
| 4 | variable (0/1/2 children) | branch/load/store/control |
| 5 | base \| imm14<<14 \| imm14 | waveform table addressing |

Proposal:
```cpp
enum class OpcodeFormat : int {
    NoArg         = 0,
    RegImm20      = 1,
    RegTripleImm8 = 2,
    DualRegImm20  = 3,
    Complex       = 4,
    DualImm14     = 5
};
```

### A9. `NodeMapItem::typeIdx` — node value encoding type

| Value | Meaning | Encoding |
|-------|---------|----------|
| 0 | IntegerPassthrough | direct register / int value |
| 1 | SinePair           | dual 32-bit I+Q (suser 0x17 + 0x19) |
| 2 | FloatBits          | IEEE-754 single-precision bit pattern |
| 3 | RawDoubleLow32     | double low 32 bits + high32 via second write |
| 4 | Frequency          | Hz → 48-bit fixed-point via `toFrequency()` |
| 5 | Phase              | degrees → 23-bit fixed-point via `toPhase()` |

Canonical enum:
```cpp
enum class NodeTypeIdx : int32_t {
    IntegerPassthrough = 0,
    SinePair           = 1,
    FloatBits          = 2,
    RawDoubleLow32     = 3,
    Frequency          = 4,
    Phase              = 5
};
```

The write-to-node switch in the `custom_functions_play` source is
the authoritative mapping from `typeIdx` to semantic name.

### A10. `Assembler::getCmdType()` return values

| Value | Bits | Meaning | Instructions |
|-------|------|---------|--------------|
| 0 | 000 | No register access     | NOP, JMP, control-flow |
| 1 | 001 | Reads registers        | PRF, WVF, BRZ, ST, etc. |
| 2 | 010 | Writes dest register   | LD |
| 3 | 011 | Reads and writes       | ALU-immediate |
| 7 | 111 | Reg-reg (reg1 is both src+dst) | ADDR..XORR |

Proposal:
```cpp
enum CmdType : int {
    CmdType_None      = 0,
    CmdType_Read      = 1,
    CmdType_Write     = 2,
    CmdType_ReadWrite = 3,
    CmdType_RegReg    = 7
};
```

### A11. `Assembler::getRegisterOrder()` return values

| Value | Meaning | Operand assignment |
|-------|---------|--------------------|
| 0 | No registers     | — |
| 1 | Source-only      | one reg → reg2 |
| 2 | Dest-only        | one reg → reg0 |
| 3 | Three-register   | two regs → reg1, reg2 |
| 4 | Dest + imm-source | two regs → reg0, reg2 |

Proposal:
```cpp
enum class RegOrder : int {
    None       = 0,
    SourceOnly = 1,
    DestOnly   = 2,
    ThreeReg   = 3,
    DestImmSrc = 4
};
```

### A12. `getNextActionForReg()` return values

| Value | Meaning |
|-------|---------|
| 0 | Not found              |
| 1 | Read (bit 0)           |
| 2 | Written (bit 1)        |
| 3 | Both / branch          |

Proposal:
```cpp
enum RegAction : int {
    NotFound    = 0,
    Read        = 1,
    Written     = 2,
    ReadWritten = 3
};
```

### A13. `Assembler::getCycles()` return values

| Value | Meaning |
|-------|---------|
| 0 | Unknown         |
| 1 | Single-cycle    |
| 3 | Branch penalty  |

Proposal:
```cpp
constexpr int kCycle_Unknown       = 0;
constexpr int kCycle_Single        = 1;
constexpr int kCycle_BranchPenalty = 3;
```

### A14. Suser (user register) address constants

30 unique addresses used in `suser()` calls.  Only `0x44`
(`SyncRegA`) and `0x45` (`SyncRegB`) currently have names in
`device_constants.hpp`; the remainder appear as bare hex.

| Addr | Name (proposed) | Semantic |
|------|----------------|----------|
| 0x00 | GenericUser0     | Generic user reg 0 (HDAWG sync) |
| 0x10 | WriteLow         | Multi-word write: 1st word |
| 0x11 | WriteMid         | Multi-word write: 2nd word |
| 0x12 | WriteHigh        | Multi-word write: 3rd word (commit) |
| 0x13 | WriteHigh2       | Double-precision high 32 bits |
| 0x16 | WriteCommit      | Commit/finalize node write (already named `kSuserNodeCommit`) |
| 0x17 | DirectWrite      | Single-value direct write |
| 0x19 | DirectWriteB     | Companion for sine/stereo Q channel |
| 0x1A | TriggerValue     | Trigger value load |
| 0x1C | NowTimestamp     | Current sequencer timestamp |
| 0x1D | RTLoggerData     | RT logger output |
| 0x44 | SyncRegA         | Sync register A (already named) |
| 0x45 | SyncRegB         | Sync register B (already named) |
| 0x69 | WaitCycles       | Wait-cycles / AWG-core-count |
| 0x6E | SyncHirzel       | Sync register (Hirzel variant) |
| 0x6F | WaitLegacy       | Wait register (legacy/commented) |
| 0x70 | SinePhase0       | Sine phase, oscillator 0 |
| 0x71 | SinePhase1       | Sine phase, oscillator 1 |
| 0x72 | SinePhaseInc0    | Sine phase increment, osc 0 |
| 0x73 | SinePhaseInc1    | Sine phase increment, osc 1 |
| 0x74 | PRNGSeed         | PRNG seed register |
| 0x75 | PRNGRangeLo      | PRNG range register (lo) |
| 0x76 | PRNGRangeHi      | PRNG range register (hi) |
| 0x78 | QAWeightsAddr    | QA integration weights + result addr |
| 0x79 | QATriggerMonitor | QA trigger/monitor composite |
| 0x7A | QAResultLength   | QA result length (UHFQA) |
| 0x8C | SweepOscIndex    | Freq sweep oscillator index |
| 0x8D | SweepControl     | Freq sweep control register |
| 0x8E | SweepStartLo     | Freq sweep start freq low 32 |
| 0x8F | SweepStartHi     | Freq sweep start freq high 32 |
| 0x90 | SweepStepLo      | Freq sweep step freq low 32 |
| 0x91 | SweepStepHi      | Freq sweep step freq high 32 |

\see \ref notes_special_registers for the runtime semantics of
these registers; \ref notes_custom_functions for the SeqC
built-ins that emit reads / writes to them.

### A15. Sentinel value `-1` for "inherit/unset"

| Context              | Proposed constant                            |
|----------------------|----------------------------------------------|
| Rate = inherit global | `constexpr int kRateInherit  = -1;`          |
| No waveform index     | `constexpr int kNoWaveIndex   = -1;`         |
| No node ID            | `constexpr int kNoNodeId      = -1;`         |
| No play index         | `constexpr int kNoPlayIndex   = -1;`         |

### A16. I/Q channel tags in writeToNode

| Value | Meaning |
|-------|---------|
| 0x0C  | I channel |
| 0x0D  | Q channel |

Proposal:
```cpp
constexpr int kChannelTag_I = 0x0C;
constexpr int kChannelTag_Q = 0x0D;
```

---

## Category B: Existing enums used as bare integers at call sites

These are places where the enum IS defined but call sites use raw
integer literals or `static_cast<int>` comparisons instead of the
symbolic names.

### B1. `VarType` — many call sites

Enum: `VarType_Var=2`, `VarType_String=3`, `VarType_Const=4`,
`VarType_Wave=5`, `VarType_Cvar=6`.

Call sites use `VarType(2)`, `static_cast<int>(...) == 2`, and
bitwise tricks like `(int(v) & ~1) == 4` (tests "Const or Cvar"
by masking bit 1).

Proposal: replace bare ints with enum names; add helper:
```cpp
inline bool isConstLike(VarType v) {
    return v == VarType_Const || v == VarType_Cvar;
}
```

### B2. `AwgDeviceType` — many call sites

Uses `static_cast<int>(deviceType) == 2` instead of `HDAWG`, etc.

### B3. `DeviceTypeCode`

16 numeric case labels in the `toAwgDeviceType` switch.

### B4. `DeviceOption`

31 numeric case labels in `toString(DeviceOption, ...)`.

### B5. `AwgSequencerType`

`case 0/1/2` instead of `Auto/QA/SG`.

### B6. `SubFunc`

`switch(static_cast<int>(subFunc))` cases dispatched by the
`SubFunc` enum (`Default=1, Aux=2, Now=3, DigTrigger=4`).

### B7. `NodeType`

`n->type != 4` instead of `NodeType::Branch`, etc.

### B8. `ValueType`

`static_cast<ValueType>(3)` instead of `ValueType::Double`, etc.

### B9. `Assembler::INVALID`

`static_cast<Assembler::Command>(0xFFFFFFFF)` or `cmd == -1`
instead of `Assembler::INVALID`.

### B10. `ErrorMessageT` — 109 distinct bare hex error IDs

All 109 values used as `static_cast<ErrorMessageT>(0xNN)` have
named enum entries.

### B11. `Cache::unusedCacheLine`

Uses literal `0xFFFFFFFFu` instead of the existing named
constant.

---

## Category C: Documented but not yet reconstructed

### C1. `debugFlags` bits (all commented out)

| Bit | Mask | Purpose |
|-----|------|---------|
| 0 | 0x01 | Unknown / reserved (never tested) |
| 1 | 0x02 | Print old (pre-SeqC) AST |
| 2 | 0x04 | Print SeqC AST |
| 3 | 0x08 | Print instruction tree / assembly |

---

## Category D: Cleanup-only (not semantic)

### D1. `hasPrecomp & 0x1` — redundant mask on bool

`hasPrecomp` is `bool` in `device_constants.hpp`.  Several call
sites carry an `& 0x1` mask which is a disassembly artefact of
the compiler's `test $1, %al` codegen for booleans.  Can be
simplified to plain `bool` usage.
