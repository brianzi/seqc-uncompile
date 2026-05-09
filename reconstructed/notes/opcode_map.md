# Opcode Map

Determined from disassembly of `AsmCommands`, `AsmCommandsImpl*` methods,
and the global `cmdMap` at 0xb84c20 (43 entries, `map<string, Command>`).

## Command→String Mapping (cmdMap)

| String   | Value        | Enum Name  |
|----------|-------------|------------|
| `end`    | 0x00000000  | END        |
| `nop`    | 0x00000001  | NOP        |
| `msg`    | 0x00000003  | MESSAGE    |
| `rer`    | 0x00000005  | ERROR_MSG  |
| `prf`    | 0x10000000  | PRF        |
| `wvf`    | 0x20000000  | WVF        |
| `wvfi`   | 0x30000000  | WVFI       |
| `wvfs`   | 0x30000001  | WVFS_H     |
| `addi`   | 0x40000000  | ADDI       |
| `addiu`  | 0x50000000  | ADDIU      |
| `addr`   | 0x60000000  | ADDR       |
| `subr`   | 0x60000001  | SUBR       |
| `andr`   | 0x60000002  | ANDR       |
| `orr`    | 0x60000003  | ORR        |
| `xnorr`  | 0x60000004  | XNORR      |
| `ssl`    | 0x60000005  | SSL        |
| `ssr`    | 0x60000006  | SSR        |
| `xorr`   | 0x60000007  | XORR       |
| `andi`   | 0x70000000  | ANDI       |
| `andiu`  | 0x80000000  | ANDIU      |
| `ori`    | 0x90000000  | ORI        |
| `oriu`   | 0xA0000000  | ORIU       |
| `xnori`  | 0xB0000000  | XNORI      |
| `xnoriu` | 0xC0000000  | XNORIU     |
| `ld`     | 0xD0000000  | LD         |
| `wtrig`  | 0xE0000000  | WTRIG      |
| `wprf`   | 0xF0000000  | WPRF       |
| `wwvfq`  | 0xF0000000  | WPRF (alias) |
| `wwvf`   | 0xF1000000  | WWVF       |
| `cwvf`   | 0xF2000000  | CWVF       |
| `brz`    | 0xF3000000  | BRZ        |
| `brnz`   | 0xF4000000  | BRNZ       |
| `brgz`   | 0xF5000000  | BRGZ       |
| `st`     | 0xF6000000  | ST         |
| `trap`   | 0xF7000000  | TRAP       |
| `irpt`   | 0xF8000000  | IRPT       |
| `cwvfr`  | 0xF9000000  | CWVFR      |
| `wvfe`   | 0xFA000000  | WVFE       |
| `wvfei`  | 0xFB000000  | WVFEI      |
| `wvfet`  | 0xFC000000  | WVFET      |
| `wtrigi` | 0xFD000000  | WTRIGI     |
| `jmp`    | 0xFE000000  | JMP        |
| `fb`     | 0xFF000000  | FB         |

Notes:
- `wprf` and `wwvfq` are aliases — both map to 0xF0000000
- `commandFromString` is case-insensitive (uses `boost::to_lower_copy`)
- `commandToString` does O(n) reverse scan (map is keyed by string)
- LABEL (0x02) is NOT in cmdMap — it's a pure pseudo-instruction

## Mnemonic interpretations

The interpretations below were derived by tracing each mnemonic from its
factory in `AsmCommands` / `AsmCommandsImpl*` back to the SeqC frontend
caller (custom function, AST eval, codegen pass) that emits it. They
supersede earlier guesses; in particular several previous notes were
wrong (see "Corrections" at the end of this section).

Confidence:
- **verified** — the operation is pinned by frontend caller behaviour
  and the letter expansion is consistent with the use site.
- **derived** — operation is verified; letter expansion is the most
  consistent reading from naming patterns within the family.
- **interpretive** — operation is verified but the letter expansion
  cannot be directly proved from source; recorded as the best informed
  reading.

| Mnemonic  | Letters expand to                  | Conf.       |
|-----------|-------------------------------------|-------------|
| `end`     | end (program terminator)            | verified    |
| `nop`     | no-op                               | verified    |
| `msg`     | message (warning-level diagnostic)  | verified    |
| `rer`     | report error (error-level diagnostic) | interpretive |
| `prf`     | prefetch                            | verified    |
| `wprf`    | wait for prefetch (barrier)         | verified    |
| `wwvf`    | wait for waveform (barrier)         | verified    |
| `wwvfq`   | wait for waveform queue (barrier)   | derived     |
| `wvf`     | waveform (play)                     | verified    |
| `wvfi`    | waveform, indexed                   | verified    |
| `wvfs`    | waveform set / setup (fetch descriptor) | derived |
| `wvft`    | waveform table (execute table entry) | derived    |
| `wvfe`    | waveform, extended-opcode form of `wvf`     | derived |
| `wvfei`   | waveform, extended-opcode form of `wvfi`    | derived |
| `wvfet`   | waveform, extended-opcode form of `wvft`    | derived |
| `cwvf`    | configure waveform (writes PlayConfig register) | verified |
| `cwvfr`   | configure waveform, register operand | verified   |
| `wtrig`   | wait for trigger                    | verified    |
| `wtrigi`  | wait for trigger, immediate         | derived     |
| `fb`      | feedback (configure feedback processing) | verified |
| `brz`     | branch if zero                      | verified    |
| `brnz`    | branch if not zero                  | verified    |
| `brgz`    | branch if greater than zero         | verified    |
| `jmp`     | jump (unconditional)                | verified    |
| `trap`    | trap                                | verified    |
| `irpt`    | interrupt                           | derived     |
| `ld`      | load                                | verified    |
| `st`      | store                               | verified    |
| `addr`    | add, register operand               | verified    |
| `subr`    | subtract, register operand          | verified    |
| `andr`    | and, register operand               | verified    |
| `orr`     | or, register operand                | verified    |
| `xorr`    | xor, register operand               | verified    |
| `xnorr`   | xnor, register operand              | verified    |
| `ssl`     | single-bit shift left               | derived     |
| `ssr`     | single-bit shift right              | derived     |
| `addi`    | add, immediate (low 12 bits)        | verified    |
| `addiu`   | add, immediate, upper word (imm << 12) | verified |
| `andi` / `andiu` | and, immediate (low / upper)  | derived     |
| `ori` / `oriu`   | or, immediate (low / upper)   | derived     |
| `xnori` / `xnoriu` | xnor, immediate (low / upper) | derived  |

### Naming conventions

- `w` prefix on `wprf` / `wwvf` / `wwvfq` / `wtrig` / `wtrigi` = "**w**ait
  for X" — these are synchronisation barriers, not "write" operations.
- `r` suffix on ALU mnemonics (`addr`, `subr`, `andr`, `orr`, `xorr`,
  `xnorr`) = "**r**egister operand" form (3-register, dst = dst OP src).
- `i` suffix on ALU mnemonics (`addi`, `andi`, `ori`, `xnori`) =
  "**i**mmediate operand" form (low 12 bits of a wide immediate).
- `u` suffix on ALU-immediate mnemonics (`addiu`, `andiu`, `oriu`,
  `xnoriu`) = "**u**pper word" — the immediate is shifted left by 12
  before being applied. Chained after the matching low-bits form to
  encode wide constants. Not "unsigned" as earlier notes claimed.
- `i` suffix on `wtrigi` = "**i**mmediate operand".
- `i` suffix on `wvfi` = "**i**ndexed" (uses an index register).
- `e` infix on `wvfe` / `wvfei` / `wvfet` = "**e**xtended-opcode form"
  — these live in the `0xF...` extended-opcode block, while their
  non-`e` siblings (`wvf`, `wvfi`) live in the short-opcode block. The
  extended encoding gives more bits for an immediate at the cost of
  fewer register slots (regOrder=1 vs 3).
- `r` suffix on `cwvfr` = "**r**egister operand" (spillover form when
  the packed PlayConfig value won't fit in the 24-bit immediate of
  `cwvf`).

### Frontend emission summary

| Mnemonic | Emitted by                                                     |
|----------|----------------------------------------------------------------|
| `prf`/`wprf` | `prefetch_*.cpp` — cache prefetch + wait-for-prefetch barrier |
| `wwvf`   | `waitWave` custom fn; program-trailer drain in `compiler.cpp` |
| `wwvfq`  | `waitPlayQueueEmpty` custom fn (Hirzel-only)                  |
| `cwvf`/`cwvfr` | `prefetch_emit.cpp` / `prefetch_placesingle.cpp` — writes the PlayConfig register packed by `PlayConfig::encodeCwvf()` |
| `wvf`/`wvfi` | `Prefetch::wvfRegImpl` — non-indexed vs indexed waveform play |
| `wvfs`   | `Prefetch::wvfs` — waveform-fetch descriptor with 20-bit offset |
| `wvft`   | `playWaveDIO`, `playZSync`, `executeTableEntry` custom fns    |
| `wtrig`  | `waitDigTrigger`, `waitDIOTrigger`, `waitForTrigger`, etc.    |
| `fb`     | `configureFeedbackProcessing` (Hirzel-only)                   |
| `brz`/`brnz`/`brgz` | AST control-flow eval (`if`, `while`, `for`, `repeat`, `do…while`) and prefetch_splitplay loop tails |
| `trap`   | `play` validation paths in `custom_functions_play.cpp`        |
| `nop`    | wait family + program trailer in `compiler.cpp`               |
| `ld`     | DIO read paths (`getDIO`, `getDIOTriggered`, `getInternalTrigger`) |
| `st`     | `setUserReg`, `setSync*`, `setupTrigger`, `setupTimestamp`    |
| `addi`   | every wait/trigger/feedback/PRNG path that loads an immediate into a register |
| `addiu`  | `Prefetch::wvfs` — chained after `addi` to extend an address into the upper word |
| `addr`/`subr`/`andr` | AST arithmetic eval and prefetch index arithmetic |
| `ssl`    | `Prefetch::wvfRegImpl`, `prefetch_splitplay`, `prefetch_placesingle` — single-bit shift used as part of address arithmetic between `addi` and `wvf`/`wvfi` |
| `msg`/`rer` | parsed only from hand-written `.asm` source by the standalone `AWGAssemblerImpl` pipeline (flex/bison `asm_lexer.l` + `asm_parser.y`); never emitted by the SeqC compiler frontend. Consumed by `AsmOptimize::reportUserMessages` and routed to the warning / error callbacks. |
| `wvfe`/`wvfei`/`wvfet` | `wvfe` produced by Hirzel `wvf()` factory when `dstReg == R0`; `wvfet` produced by Hirzel `wvft()` factory (always); `wvfei` has no emitter (exists in enum/parser for symmetry) |
| `irpt`/`jmp`/`wtrigi` | not emitted by any current frontend factory; `jmp` is produced internally by Hirzel `brz()` when `reg == R0` |

### Corrections to earlier notes

- `cwvf` was previously labelled "cancel waveform" — it actually
  **writes the PlayConfig register** (rate / suppress / marker bits /
  channel mask), packed by `PlayConfig::encodeCwvf()`. `cwvfr` is the
  register-operand spillover when the packed value won't fit in the
  24-bit immediate of `cwvf`.
- `addiu` was previously labelled "add immediate unsigned" — it is
  actually "add immediate, **upper word**" (`imm << 12`). Same `u`
  semantics for `andiu` / `oriu` / `xnoriu`.
- `wvfi` was previously labelled "interleaved" — it is "**indexed**"
  (uses an index register; the indexed-vs-non-indexed branch in
  `Prefetch::wvfRegImpl` is the proof).
- `wprf` / `wwvf` / `wwvfq` / `wtrig` were previously read as "write
  X" — they are all "**wait for X**" synchronisation barriers.
- `wvfe` / `wvfei` / `wvfet` were previously labelled "waveform end /
  end interleaved / end triggered". They are the **extended-opcode
  form** of `wvf` / `wvfi` / `wvft` (single-register variants in the
  `0xF...` block). The `e` denotes the extended encoding, not "end".
- `isCWVFRSupported()` is a Hirzel-vs-Cervino capability flag. It
  gates marker-bit computation in `AsmCommands::genPlayConfig` and the
  Hirzel-only `cwvfr`, `wvfs`, `wvft`, `wwvfq` factories. The name
  reflects the most prominent gated opcode (CWVFR), not the full set
  of capabilities behind the flag.

## Control / Pseudo-instructions

| Opcode       | Name      | Args                | Notes                          |
|--------------|-----------|---------------------|--------------------------------|
| `0x00000000` | END       | (none)              | End of program                 |
| `0x00000001` | NOP       | (none)              | No operation                   |
| `0x00000002` | LABEL     | label: string       | Pseudo — label definition      |
| `0x00000003` | MESSAGE   | msg: string         | `msg` — warning-level diagnostic; only producible via the standalone `AWGAssemblerImpl` parsing a hand-written `.asm` source. No binary code. |
| `0x00000005` | ERROR_MSG | msg: string         | `rer` — error-level diagnostic; same provenance as MESSAGE. No binary code. |

## Waveform Playback / Prefetch / Configuration

| Opcode       | Name    | Args                        | Device  | Notes                     |
|--------------|---------|-----------------------------|---------|---------------------------|
| `0x10000000` | PRF     | reg_h, reg_c, imm           | Both    | Prefetch into cache (Hirzel-cache reg, Cervino-cache reg, length) |
| `0x20000000` | WVF     | reg_wave, reg_marker, imm   | Both    | Play waveform — short-opcode form, 3-reg |
| `0x30000000` | WVFI    | reg_wave, reg_index, imm    | Cervino | Play waveform, indexed (uses index register) |
| `0x30000001` | WVFS_H  | imm_dummy, reg, imm         | Hirzel  | Set up waveform-fetch descriptor (PlayDummyType + 20-bit offset) |
| `0xF0000000` | WPRF    | (none)                      | Both    | **Wait for prefetch** — barrier paired with `prf`. No-op on Hirzel (sentinel 0xFFFFFFFF) |
| `0xF1000000` | WWVF    | (none)                      | Both    | **Wait for waveform** — barrier; appended to program trailer and by `waitWave` |
| `0xF2000000` | CWVF    | imm                         | Both    | **Configure waveform** — writes PlayConfig register (rate / suppress / marker bits / channel mask) packed via `PlayConfig::encodeCwvf()`. 24-bit immediate. |
| `0xF9000000` | CWVFR   | reg                         | Hirzel  | Configure waveform, **register operand** — spillover form when packed value ≥ 0xFFFFFF |
| `0xFA000000` | WVFE    | reg_wave, imm               | Hirzel  | **Extended-opcode form** of WVF (single-register; emitted by Hirzel `wvf()` when dstReg == R0) |
| `0xFB000000` | WVFEI   | (no emitter)                | Hirzel  | **Extended-opcode form** of WVFI; present in enum/parser, never produced by codegen |
| `0xFC000000` | WVFET   | reg, imm                    | Hirzel  | **Extended-opcode form** of WVFT (execute table entry; emitted by Hirzel `wvft()` factory). Used by `playWaveDIO`, `playZSync`, `executeTableEntry` |
| `0xFF000000` | FB      | imm                         | Hirzel  | **Configure feedback** processing (mode / shift / numBits / threshold packed in immediate) |

## ALU — Register-Register

`r` suffix throughout = "register operand" form (3-register `dst = dst OP src`).
SSL/SSR are single-bit shifts with **no count operand** — the shift amount is fixed at 1.

| Opcode       | Name  | Args          | Notes       |
|--------------|-------|---------------|-------------|
| `0x60000000` | ADDR  | dst, src      | Add         |
| `0x60000001` | SUBR  | dst, src      | Subtract    |
| `0x60000002` | ANDR  | dst, src      | Bitwise AND |
| `0x60000003` | ORR   | dst, src      | Bitwise OR  |
| `0x60000004` | XNORR | dst, src      | Bitwise XNOR|
| `0x60000005` | SSL   | reg [, reg2]  | Single-bit shift left (Cervino: reg,reg in-place; Hirzel: reg,R0). Used by prefetch address arithmetic. |
| `0x60000006` | SSR   | reg [, reg2]  | Single-bit shift right (same encoding pattern as SSL). |
| `0x60000007` | XORR  | dst, src      | Bitwise XOR |

## ALU — Immediate (low-bits / upper-word)

`i` suffix = "immediate operand" (low 12 bits).
`iu` suffix = "immediate, upper word" — the immediate is shifted left by 12 before being applied. Chained after the matching low-bits form to encode wide constants.

| Opcode       | Name   | Args             | Notes                    |
|--------------|--------|------------------|--------------------------|
| `0x40000000` | ADDI   | dst, src, imm    | Add immediate (low 12 bits) |
| `0x50000000` | ADDIU  | dst, src, imm    | Add immediate, upper word (imm << 12) — chained after ADDI for wide constants |
| `0x70000000` | ANDI   | dst, src, imm    | AND immediate (low 12 bits) |
| `0x80000000` | ANDIU  | dst, src, imm    | AND immediate, upper word |
| `0x90000000` | ORI    | dst, src, imm    | OR immediate (low 12 bits) |
| `0xA0000000` | ORIU   | dst, src, imm    | OR immediate, upper word |
| `0xB0000000` | XNORI  | dst, src, imm    | XNOR immediate (low 12 bits) |
| `0xC0000000` | XNORIU | dst, src, imm    | XNOR immediate, upper word |

Wide signed immediates (> ~18-bit) are split across multiple instructions:
ADDI (low 12 bits) followed by an ADDIU that adds the upper bits shifted left by 12.

## Load / Store / IO / Trigger

| Opcode       | Name   | Args         | Notes                         |
|--------------|--------|--------------|-------------------------------|
| `0xD0000000` | LD     | reg, addr    | Load from memory-mapped register |
| `0xE0000000` | WTRIG  | reg1, reg2   | **Wait for trigger** (2 registers — trigger value pre-loaded via `addi`) |
| `0xF6000000` | ST     | reg, addr    | Store to memory-mapped register |
| `0xFD000000` | WTRIGI | imm          | Wait for trigger, immediate operand (no current frontend emitter) |

### Known Memory-Mapped Addresses

| Address | Access  | Function                        |
|---------|---------|----------------------------------|
| `0x20`  | LD/ST   | Digital I/O (low bank)           |
| `0x21`  | ST      | Set ID (low bank)                |
| `0x22`  | LD/ST   | Trigger register                 |
| `0x23`  | ST      | Internal trigger                 |
| `0x40+` | ST      | Wtrig LS placeholder (value+0x40)|
| `0x60`  | LD      | I/O trigger (Cervino)            |
| `0x68`  | LD      | I/O trigger (Hirzel)             |
| `0x6E`  | ST      | Sync (Hirzel, via suser)         |
| `0x1FE` | LD/ST   | Digital I/O (high bank)          |
| `0x1FF` | ST      | Set ID (high bank)               |

## Branch

| Opcode       | Name   | Args              | Device  | Notes                    |
|--------------|--------|-------------------|---------|-----------------------------|
| `0xF3000000` | BRZ    | reg, label        | Both    | Branch if zero              |
| `0xF4000000` | BRNZ   | reg, label        | Both    | Branch if not zero          |
| `0xF5000000` | BRGZ   | reg?, label       | Both?   | Branch if greater than zero |
| `0xFE000000` | JMP    | label             | Hirzel  | Unconditional jump (was BRZ_H) |

## Control

| Opcode       | Name | Args   | Notes         |
|--------------|------|--------|---------------|
| `0xF7000000` | TRAP | (none) | Trap          |
| `0xF8000000` | IRPT | (none) | Interrupt     |

## Scheduling Classification (from getOpcodeType / getCycles / getCmdType / getRegisterOrder)

| Command     | opcodeType | cycles | cmdType | regOrder | Notes |
|-------------|-----------|--------|---------|----------|-------|
| NOP         | 3         | 1      | 0       | 0        |       |
| PRF         | 3         | 1      | 1       | 3        | 3-reg |
| WVF         | 3         | 1      | 1       | 3        | 3-reg |
| WVFI        | 3         | 1      | 1       | 3        | 3-reg |
| WVFS_H      | 3         | 1      | 1       | 1        | src-only |
| ADDI..XNORIU| 3         | 1      | 3 (imm) | 4        | dst+imm |
| ADDR..XORR  | 3         | 1      | 7 (r-r) | 3        | 3-reg |
| LD          | 1         | 1      | 2       | 2        | dst-only |
| WTRIG       | 3         | 1      | 1       | 3        | 3-reg |
| BRZ         | 4         | 3      | 1       | 1        | branch penalty |
| BRNZ        | 4         | 3      | 1       | 1        | branch penalty |
| BRGZ        | 4         | 3      | 1       | ?        | branch penalty |
| ST          | 4         | 1      | 1       | 1        | src-only |
| JMP         | 4         | 1      | 0       | 0        |       |
| WVFE        | 4         | 1      | 1       | 1        | src-only |
| WVFET       | 4         | 1      | 1       | 1        | src-only |

## Sentinel

| Opcode       | Name    | Notes                                    |
|--------------|---------|------------------------------------------|
| `0xFFFFFFFF` | INVALID | Used for Node-creating pseudo-entries and Hirzel wprf no-op |
