# Magic Numbers & Missing Named Constants — Reference

Date: 2026-04-24 (created), 2026-04-24 (demoted to reference)

This document catalogs all fields with discriminator/enum/type-tag or
bitfield-flag semantics where raw integer literals appear instead of
named constants. **Actionable refactoring items are tracked in
TODO.md** (see "Magic numbers refactoring" entries in the Deferred /
Low Priority section). This file provides the value tables, semantic
descriptions, and suggested enum definitions as reference material only.

---

## Category A: Fields needing NEW enum/constant definitions

These are places where no named enum or constant exists at all.

### A1. `Immediate::index_` — variant discriminator

- **Declaration:** `value.hpp:58` — `uint32_t index_`
- **Usage:** `value.cpp` — 7+ switch sites (copy/move ctors, assignment,
  destructor, `operator int`, `operator unsigned`, `operator==`, `toString`)
- **Values:**

  | Value | Meaning |
  |-------|---------|
  | 0 | Address (unsigned) |
  | 1 | Integer (signed int32) |
  | 2 | String |
  | 0xFFFFFFFF | Valueless / empty |

- **Proposal:** `enum class ImmediateKind : uint32_t { Address=0, Integer=1, String=2, Valueless=0xFFFFFFFF };`

### A2. `AsmExpression::type` — expression node type

- **Declaration:** `asm_expression.hpp:97` — `int type`
- **Usage:** `asm_list.cpp:435-461`, `awg_assembler_opcodes.cpp:104,137-161`,
  `awg_assembler_impl_pipeline.cpp:489`
- **Values:**

  | Value | Meaning |
  |-------|---------|
  | 0 | Container / arglist |
  | 1 | Register |
  | 2 | Label / name |
  | 3 | Integer / value |

- **Proposal:** `enum class AsmExprType : int { Container=0, Register=1, Label=2, Integer=3 };`

### A3. `Variable::value.which_` — variant slot discriminator

- **Declaration:** In `Variable` struct (resources.hpp)
- **Usage:** `resources.cpp` — ~10 switch/assignment sites
- **Values:**

  | Value | Meaning |
  |-------|---------|
  | 0 | int |
  | 1 | bool |
  | 2 | double |
  | 3 | string |

- **Proposal:** `enum class VariantSlot : int { Int=0, Bool=1, Double=2, String=3 };`

### A4. `Variable::flags` — variable state flags

- **Declaration:** `resources.hpp` — `uint16_t flags`
- **Usage:** `resources.cpp:524,982,1021,1068`, `resources_function.cpp:379`
- **Values:** bit 0 = Written, bit 1 = Frozen
- **Proposal:**
  ```cpp
  constexpr uint16_t VarFlag_Written = 0x01;
  constexpr uint16_t VarFlag_Frozen  = 0x02;
  ```

### A5. `AsmOptimize::optFlags_` — optimization pass bitmask

- **Declaration:** `asm_optimize.hpp` — `uint32_t optFlags_`
- **Usage:** `asm_optimize.cpp:210,226,231,237,242,505,513`
- **Values:**

  | Mask | Meaning |
  |------|---------|
  | 0x01 | oneStepJumpElimination |
  | 0x02 | removeUnusedLabels + mergeLabels |
  | 0x04 | deadCodeElimination |
  | 0x08 | mergeRegisterZeroing |
  | 0x10 | removeUnusedRegs + registerAllocation |

- **Proposal:**
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

- **Declaration:** used with `typeNames[]` array in `compiler_message.cpp:21`
- **Usage:** `compiler_message.cpp:24,28`
- **Values:** 0=Error, 1=Warning, 2=Info
- **Proposal:** `enum class MessageType : int { Error=0, Warning=1, Info=2 };`

### A7. Device factory option bits

- **Declaration:** `opts` parameter in `device_factories.hpp:51-68`
  (documented in comments only)
- **Usage:** `device_factories.cpp:85-258`, `device_shf.cpp:72-75`,
  `device_vhf.cpp:35`, `device_ghf.cpp:36`, `device_shfacc.cpp:19`
- **Values:**

  | Mask | Meaning |
  |------|---------|
  | 0x1C0 | Subtype selector mask (bits 8:6) |
  | 0x040 | Subtype slot 1 |
  | 0x080 | Subtype slot 2 |
  | 0x0C0 | Subtype slot 3 |
  | 0x100 | Subtype slot 4 |
  | 0x020 | FF option |
  | 0x2000 | RTR option |
  | 0x4000 | PLUS option |
  | 0x8000 | LRT option |

- **Proposal:** Named `constexpr unsigned long` constants for each.

### A8. Opcode encoding format categories (0–5)

- **Declaration:** dispatch in `awg_assembler_impl_pipeline.cpp:504-511`
- **Usage:** dispatches to `opcode0`..`opcode5` in
  `awg_assembler_opcodes.cpp`
- **Values:**

  | Value | Bit format | Instructions |
  |-------|-----------|--------------|
  | 0 | base unchanged | END, NOP |
  | 1 | reg<<24 \| imm20 | ADDI, ADDIU, ANDI, ANDIU, ORI, ORIU, XNORI, XNORIU |
  | 2 | reg<<24 \| 3×imm8 | PRF, WVF, WVFI, WTRIG |
  | 3 | reg1<<24 \| reg2<<20 \| imm20 | ADDR..XORR, WVFS_H |
  | 4 | variable (0/1/2 children) | branch/load/store/control |
  | 5 | base \| imm14<<14 \| imm14 | waveform table addressing |

- **Proposal:** `enum class OpcodeFormat : int { NoArg=0, RegImm20=1, RegTripleImm8=2, DualRegImm20=3, Complex=4, DualImm14=5 };`
- **Reconstruction issue:** Line 498 calls `Assembler::getOpcodeType()` for
  this dispatch, but that function returns {0,1,3,4} (scheduling
  classification). The actual binary likely uses a lookup table at
  0x95d094. This conflation should be investigated separately.

### A9. `NodeMapItem::typeIdx` — node value encoding type

- **Declaration:** `typeIdx` field in node map items
- **Usage:** `custom_functions.cpp:2101-2283, 2324-2564`
- **Values:**

  | Value | Meaning | Encoding |
  |-------|---------|----------|
  | 0 | IntegerPassthrough | direct register/int value |
  | 1 | SinePair | dual 32-bit I+Q (suser 0x17 + 0x19) |
  | 2 | FloatBits | IEEE-754 single-precision bit pattern |
  | 3 | RawDoubleLow32 | double low 32 bits + high32 via second write |
  | 4 | Frequency | Hz → 48-bit fixed-point via `toFrequency()` |
  | 5 | Phase | degrees → 23-bit fixed-point via `toPhase()` |

- **Proposal:** `enum class NodeTypeIdx : int32_t { IntegerPassthrough=0, SinePair=1, FloatBits=2, RawDoubleLow32=3, Frequency=4, Phase=5 };`
- **Note:** `notes/writeToNode_block_d_protocol.md` has cases 1 and 4
  swapped relative to the .cpp source. The .cpp is authoritative.
  **Tracked in TODO.md as the typeIdx reconciliation item.**

### A10. `Assembler::getCmdType()` return values

- **Declaration:** `assembler.cpp:216-254`
- **Usage:** `asm_optimize.cpp` — 6 call sites using `& 1` (reads?),
  `& 2` (writes?), `== 7` (reg-reg)
- **Values:**

  | Value | Bits | Meaning | Instructions |
  |-------|------|---------|--------------|
  | 0 | 000 | No register access | NOP, JMP, control-flow |
  | 1 | 001 | Reads registers | PRF, WVF, BRZ, ST, etc. |
  | 2 | 010 | Writes dest register | LD |
  | 3 | 011 | Reads and writes | ALU-immediate |
  | 7 | 111 | Reg-reg (reg1 is both src+dst) | ADDR..XORR |

- **Proposal:**
  ```cpp
  enum CmdType : int {
      CmdType_None=0, CmdType_Read=1, CmdType_Write=2,
      CmdType_ReadWrite=3, CmdType_RegReg=7
  };
  ```

### A11. `Assembler::getRegisterOrder()` return values

- **Declaration:** `assembler.cpp:263-300`
- **Usage:** `asm_list.cpp:488` — single switch site
- **Values:**

  | Value | Meaning | Operand assignment |
  |-------|---------|--------------------|
  | 0 | No registers | — |
  | 1 | Source-only | one reg → reg2 |
  | 2 | Dest-only | one reg → reg0 |
  | 3 | Three-register | two regs → reg1, reg2 |
  | 4 | Dest + imm-source | two regs → reg0, reg2 |

- **Proposal:** `enum class RegOrder : int { None=0, SourceOnly=1, DestOnly=2, ThreeReg=3, DestImmSrc=4 };`

### A12. `getNextActionForReg()` return values

- **Declaration:** `asm_optimize.cpp:117-158`
- **Usage:** optimizer passes in `asm_optimize.cpp`
- **Values:** 0=not found, 1(bit 0)=read, 2(bit 1)=written, 3=both/branch
- **Proposal:** `enum RegAction : int { NotFound=0, Read=1, Written=2, ReadWritten=3 };`

### A13. `Assembler::getCycles()` return values

- **Declaration:** `assembler.cpp:160-206`
- **Values:** 0=unknown, 1=single-cycle, 3=branch penalty
- **Proposal:**
  ```cpp
  constexpr int kCycle_Unknown       = 0;
  constexpr int kCycle_Single        = 1;
  constexpr int kCycle_BranchPenalty = 3;
  ```

### A14. Suser (user register) address constants

30 unique addresses used in `suser()` calls. Only 0x44 (SyncRegA) and
0x45 (SyncRegB) have names in `device_constants.hpp`. All others are
bare hex.

**Complete table:**

| Addr | Name (proposed) | Semantic | Source locations |
|------|----------------|----------|-----------------|
| 0x00 | GenericUser0 | Generic user reg 0 (HDAWG sync) | `custom_functions.cpp:6010` |
| 0x10 | WriteLow | Multi-word write: 1st word | `custom_functions.cpp:2323,2353,2587,5994` |
| 0x11 | WriteMid | Multi-word write: 2nd word | `custom_functions.cpp:2361,6001` |
| 0x12 | WriteHigh | Multi-word write: 3rd word (commit) | `custom_functions.cpp:2374,2421,2440,2528,2560` |
| 0x13 | WriteHigh2 | Double-precision high 32 bits | `custom_functions.cpp:2489` |
| 0x16 | WriteCommit | Commit/finalize node write | notes only (not yet in .cpp) |
| 0x17 | DirectWrite | Single-value direct write | `custom_functions.cpp:2122-2302` (many) |
| 0x19 | DirectWriteB | Companion for sine/stereo Q channel | `custom_functions.cpp:2178,2194` |
| 0x1A | TriggerValue | Trigger value load | `custom_functions.cpp:4438,4460` |
| 0x1C | NowTimestamp | Current sequencer timestamp | `custom_functions.cpp:3758` |
| 0x1D | RTLoggerData | RT logger output | `custom_functions.cpp:6214,6232` |
| 0x44 | SyncRegA | Sync register A (EXISTS) | `asm_commands.cpp:707,719,725` |
| 0x45 | SyncRegB | Sync register B (EXISTS) | `asm_commands.cpp:745,751` |
| 0x69 | WaitCycles | Wait-cycles / AWG-core-count | `custom_functions.cpp` (8+ sites) |
| 0x6E | SyncHirzel | Sync register (Hirzel variant) | `asm_commands.cpp:815` |
| 0x6F | WaitLegacy | Wait register (legacy/commented) | `custom_functions.cpp:732-733` |
| 0x70 | SinePhase0 | Sine phase, oscillator 0 | `custom_functions.cpp:5378,5425` |
| 0x71 | SinePhase1 | Sine phase, oscillator 1 | `custom_functions.cpp:5383` |
| 0x72 | SinePhaseInc0 | Sine phase increment, osc 0 | `custom_functions.cpp:5501,5541` |
| 0x73 | SinePhaseInc1 | Sine phase increment, osc 1 | `custom_functions.cpp:5506` |
| 0x74 | PRNGSeed | PRNG seed register | `custom_functions.cpp:6639,6662` |
| 0x75 | PRNGRangeLo | PRNG range register (lo) | `custom_functions.cpp:6731` |
| 0x76 | PRNGRangeHi | PRNG range register (hi) | `custom_functions.cpp:6743` |
| 0x78 | QAWeightsAddr | QA integration weights + result addr | `custom_functions.cpp:6837` |
| 0x79 | QATriggerMonitor | QA trigger/monitor composite | `custom_functions.cpp:6851` |
| 0x7A | QAResultLength | QA result length (UHFQA) | `custom_functions.cpp:6862` |
| 0x8C | SweepOscIndex | Freq sweep oscillator index | `custom_functions.cpp:6947,7042,7137` |
| 0x8D | SweepControl | Freq sweep control register | `custom_functions.cpp:7010,7114` |
| 0x8E | SweepStartLo | Freq sweep start freq low 32 | `custom_functions.cpp:6930,7123` |
| 0x8F | SweepStartHi | Freq sweep start freq high 32 | `custom_functions.cpp:6930,7123` |
| 0x90 | SweepStepLo | Freq sweep step freq low 32 | `custom_functions.cpp:6937` |
| 0x91 | SweepStepHi | Freq sweep step freq high 32 | `custom_functions.cpp:6937` |

### A15. Sentinel value `-1` for "inherit/unset"

Multiple fields use `-1` as a sentinel with no named constant:

| Context | Usage sites | Proposed constant |
|---------|------------|-------------------|
| Rate = inherit global | `prefetch.cpp:107,322,511,523,564`, `prefetch_emit.cpp:489,599` | `constexpr int kRateInherit = -1` |
| No waveform index | `wavetable_front.cpp:344` | `constexpr int kNoWaveIndex = -1` |
| No node ID | `node.cpp:663,678` | `constexpr int kNoNodeId = -1` |
| No play index | `wave_index_tracker.cpp:159` | `constexpr int kNoPlayIndex = -1` |

### A16. I/Q channel tags in writeToNode

- **Usage:** `custom_functions.cpp:2400-2402`
- **Values:** 0xC = I channel, 0xD = Q channel
- **Proposal:**
  ```cpp
  constexpr int kChannelTag_I = 0x0C;
  constexpr int kChannelTag_Q = 0x0D;
  ```

---

## Category B: Existing enums used as bare integers at call sites

These are places where the enum IS defined but call sites use raw
integer literals or `static_cast<int>` comparisons instead of the
symbolic names.

### B1. `VarType` — ~100+ sites in `custom_functions.cpp`

Enum defined: `VarType_Var=2`, `VarType_String=3`, `VarType_Const=4`,
`VarType_Wave=5`, `VarType_Cvar=6`.

Call sites use `VarType(2)`, `static_cast<int>(...) == 2`, and bitwise
tricks like `(int(v) & ~1) == 4` (tests "Const or Cvar" by masking
bit 1).

**Proposal:** Replace bare ints with enum names. Add helper:
```cpp
inline bool isConstLike(VarType v) {
    return v == VarType_Const || v == VarType_Cvar;
}
```

### B2. `AwgDeviceType` — ~50 sites

Files: `custom_functions.cpp`, `static_resources.cpp`,
`awg_compiler_config.cpp:38-78`.

Uses `static_cast<int>(deviceType) == 2` instead of `HDAWG`, etc.

### B3. `DeviceTypeCode` — `awg_device_props.cpp:302-326`

16 numeric case labels in `toAwgDeviceType` switch.

### B4. `DeviceOption` — `device_type.cpp:409-441`

31 numeric case labels in `toString(DeviceOption, ...)`.

### B5. `AwgSequencerType` — `awg_device_props.cpp:352-355`

`case 0/1/2` instead of `Auto/QA/SG`.

### B6. `SubFunc` — `custom_functions.cpp:1064,1253`

`switch(static_cast<int>(subFunc))` with bare cases 0–3. **Caution:**
The enum defines `Default=1, Aux=2, Now=3, DigTrigger=4`, but the
switches use cases 0–3. Either the enum values are off-by-one or the
switch has a different base. Needs investigation before replacing.

### B7. `NodeType` — `prefetch_helpers.cpp:282,518`, `prefetch_placesingle.cpp:100-104`

`n->type != 4` instead of `NodeType::Branch`, etc.

### B8. `ValueType` — `resources.cpp:520,951,978,998,1017`

`static_cast<ValueType>(3)` instead of `ValueType::Double`, etc.

### B9. `Assembler::INVALID` — 11+ sites

`static_cast<Assembler::Command>(0xFFFFFFFF)` or `cmd == -1` instead
of `Assembler::INVALID` in `asm_optimize.cpp`, `asm_list.cpp`,
`awg_assembler_impl_pipeline.cpp`.

### B10. `ErrorMessageT` — 109 distinct bare hex error IDs

All 109 values used as `static_cast<ErrorMessageT>(0xNN)` have named
enum entries. One value **0x2F (47)** is used at call sites
(`custom_functions.cpp:6590,6595`) but has no enum entry or message
string — needs binary investigation.

### B11. `Cache::unusedCacheLine` — `memory_allocator.cpp:51,66,109,143`

Uses literal `0xFFFFFFFFu` instead of the existing named constant.

---

## Category C: Incorrect/suspect enum definitions

### C1. `SeqCValue::Tag` enum ~~is wrong~~ — RESOLVED (Phase 21h.1)

- **Current definition** (`seqc_ast_node.hpp:616-618`):
  `eString=0, eDouble=1` (corrected in Phase 21h.1)
- **Previous wrong definition**: `eNone=0, eInt=1, eDouble=2, eString=3`
- **Actual binary behavior** (`seqc_ast_node.cpp:568-589`):
  - `tag_ == 0` → string payload (variant index 0)
  - `tag_ == 1` → double payload (variant index 1)
  - `tag_ == -1` (0xFFFFFFFF) → valueless/empty (default: `tag_{-1}`)
- **Root cause:** Type is `std::variant<std::string, double>` under
  libc++. Variant index 0 = string, 1 = double, -1 = valueless.
- There is no integer alternative — `eInt` and values 2/3 were
  fabricated. Fixed.

### C2. `SubFunc` enum values vs switch usage mismatch — RESOLVED (Phase 22e)

Enum: `Default=1, Aux=2, Now=3, DigTrigger=4` — **confirmed correct**.
Binary callers verified: playWave passes r8d=1, playAuxWaveIndexed passes
r8d=2, playWaveNow passes r8d=3, playWaveDigTrigger passes r8d=4.
The play() switch reconstruction was wrong (used 0-based cases); fixed to
1-based. The playIndexed() switch already used 1-based cases correctly.

---

## Category D: Documented but not yet reconstructed

### D1. `debugFlags` bits — `compiler.cpp:204-325` (all commented out)

| Bit | Mask | Purpose |
|-----|------|---------|
| 0 | 0x01 | Unknown/reserved (never tested) |
| 1 | 0x02 | Print old (pre-SeqC) AST |
| 2 | 0x04 | Print SeqC AST |
| 3 | 0x08 | Print instruction tree / assembly |

### D2. Suser address 0x16 — "commit/finalize"

Documented in `writeToNode_block_d_protocol.md:277,280,286` but not
yet present in reconstructed .cpp source.

### D3. `ErrorMessageT` value 0x2F (47) — RESOLVED (Phase 22e)

Added as `UnknownError47` enum entry. Used at custom_functions_io.cpp
(2 call sites). Message string content unknown but the binary's map
must contain the key since the code calls `ErrorMessages::get(0x2f)`.

---

## Category E: Cleanup-only (not semantic)

### E1. `hasPrecomp & 0x1` — redundant mask on bool

Field is `bool` in `device_constants.hpp:148`. The `& 0x1` mask in
`prefetch.cpp:108,318,520,560` is a disassembly artifact (compiler
`test $1, %al` codegen for bools). Can be simplified to plain bool.
