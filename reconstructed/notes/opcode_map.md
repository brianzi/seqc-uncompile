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

## Control / Pseudo-instructions

| Opcode       | Name      | Args                | Notes                          |
|--------------|-----------|---------------------|--------------------------------|
| `0x00000000` | END       | (none)              | End of program                 |
| `0x00000001` | NOP       | (none)              | No operation                   |
| `0x00000002` | LABEL     | label: string       | Pseudo — label definition      |
| `0x00000003` | MESSAGE   | msg: string         | Pseudo — emit message          |
| `0x00000005` | ERROR_MSG | msg: string         | Pseudo — emit error message    |

## Waveform Playback

| Opcode       | Name    | Args                        | Device  | Notes                     |
|--------------|---------|-----------------------------|---------|---------------------------|
| `0x10000000` | PRF     | reg_dst, reg_src, imm       | Both    | Prefetch                  |
| `0x20000000` | WVF     | reg_wave, reg_marker, imm   | Both    | Play waveform (2-reg)     |
| `0x30000000` | WVFI    | reg_wave, R0, imm           | Cervino | Play interleaved          |
| `0x30000001` | WVFS_H  | imm_dummy, reg, imm         | Hirzel  | Play special/dummy        |
| `0xF0000000` | WPRF    | (none)                      | Both    | Write playback read-fwd   |
| `0xF1000000` | WWVF    | ?                           | ?       | Write waveform            |
| `0xF2000000` | CWVF    | imm                         | Both    | Cancel waveform (imm)     |
| `0xF9000000` | CWVFR   | reg                         | Both    | Cancel waveform (reg)     |
| `0xFA000000` | WVFE    | reg_wave, imm               | Hirzel  | Waveform end (was WVF_H)  |
| `0xFB000000` | WVFEI   | ?                           | Hirzel  | Waveform end interleaved  |
| `0xFC000000` | WVFET   | reg, imm                    | Hirzel  | Waveform end triggered    |
| `0xFF000000` | FB      | imm                         | Both    | Feedback                  |

## ALU — Register-Register

| Opcode       | Name  | Args          | Notes       |
|--------------|-------|---------------|-------------|
| `0x60000000` | ADDR  | dst, src      | Add         |
| `0x60000001` | SUBR  | dst, src      | Subtract    |
| `0x60000002` | ANDR  | dst, src      | Bitwise AND |
| `0x60000003` | ORR   | dst, src      | Bitwise OR  |
| `0x60000004` | XNORR | dst, src      | Bitwise XNOR|
| `0x60000005` | SSL   | reg [, reg2]  | Shift left  (Cervino: reg,reg; Hirzel: reg,R0) |
| `0x60000006` | SSR   | reg [, reg2]  | Shift right (Cervino: reg,reg; Hirzel: reg,R0) |
| `0x60000007` | XORR  | dst, src      | Bitwise XOR |

## ALU — Immediate (Signed / Unsigned)

| Opcode       | Name   | Args             | Notes                    |
|--------------|--------|------------------|--------------------------|
| `0x40000000` | ADDI   | dst, src, imm    | Add immediate (signed)   |
| `0x50000000` | ADDIU  | dst, src, imm    | Add immediate (unsigned) |
| `0x70000000` | ANDI   | dst, src, imm    | AND immediate (signed)   |
| `0x80000000` | ANDIU  | dst, src, imm    | AND immediate (unsigned) |
| `0x90000000` | ORI    | dst, src, imm    | OR immediate (signed)    |
| `0xA0000000` | ORIU   | dst, src, imm    | OR immediate (unsigned)  |
| `0xB0000000` | XNORI  | dst, src, imm    | XNOR immediate (signed)  |
| `0xC0000000` | XNORIU | dst, src, imm    | XNOR immediate (unsigned)|

Large signed immediates (> ~18-bit) are split across multiple instructions:
ADDI (low 12 bits) + ADDIU chain (upper bits, 12 bits at a time).

## Load / Store / IO

| Opcode       | Name   | Args         | Notes                         |
|--------------|--------|--------------|-------------------------------|
| `0xD0000000` | LD     | reg, addr    | Load from memory              |
| `0xE0000000` | WTRIG  | reg1, reg2   | Write trigger (2 registers)   |
| `0xF6000000` | ST     | reg, addr    | Store to memory               |
| `0xFD000000` | WTRIGI | imm          | Write trigger (immediate)     |

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
