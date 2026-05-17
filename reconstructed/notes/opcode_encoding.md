# Opcode Encoding Formats {#notes_opcode_encoding}

The SeqC sequencer ISA is a fixed-width 32-bit encoding.  Every
instruction word follows one of six formats (`opcode0`..`opcode5`),
selected by the opcode family.  This page documents the bit layout
of each format; the per-mnemonic opcode table lives at
\ref notes_opcode_map.

## Summary of Instruction Word Formats (32-bit)

All opcodes produce a 32-bit instruction word.

### opcode0 — Zero-argument (NOP, END, etc.)
```
[31:0] = opcode_base (unchanged)
```
Children: 0 expected.  Error if any children present.

### opcode1 — 1 Register + 20-bit Immediate (ADDI, ADDIU, ANDI, etc.)
```
[31:24] = register number
[23:20] = (part of opcode — upper nibble)
[19:0]  = immediate value (20-bit, masked)
```
Children: 2 — [0]=register, [1]=immediate

### opcode2 — 1 Register + 3×8-bit Immediates (WVF, PRF-related)
```
[31:24] = register number
[23:16] = immediate_1 (8-bit)
[15:8]  = immediate_2 (8-bit)
[7:0]   = immediate_3 (8-bit)
```
Children: 4 — [0]=register, [1..3]=immediates

### opcode3 — 2 Registers + 20-bit Immediate (register-register ALU)
```
[31:24] = dest register | opcode_base_upper
[23:20] = source register
[19:0]  = immediate value (20-bit)
```
Children: 2-3 depending on format.
Special: opcode==1 → literal `0x40000000` (internal encoding trick).
Special: `0x30000001` (`WVFS_H`) uses child[0] as 1-bit selector for upper byte.

### opcode4 — Complex dispatch (branch / load / store / trap)

Variable format depending on opcode and child count:

**0 children** (opcode is self-contained):
- `0xF0000000` (WPRF), `0xF1000000` (WWVF), `0xF7000000` (TRAP),
  `0xF8000000` (IRPT), `0xFF000000` (FB)

**1 child** (immediate only):
- `0xFE000000` (JMP): `[19:0] = val(20)` → `val | 0xFE000000`
- `0xFD000000` (WTRIGI): `[4:0] = val(5)` → `val | 0xFD000000`
- Others: `val(20) | opcode_base` or `val(24) | opcode_base`

**2 children** (register + immediate):
- `0xF6000000` (ST): `reg<<20 | val(20) | 0xF6000000` (with memory
  bounds check against device memory depth)
- `0xD0`–`0xD2` (LD / WTRIG / etc.): `reg<<20 | val(20) | opcode_base`
- Others: `reg<<20 | val(20) | opcode_base`

### opcode5 — 2×14-bit Immediates (waveform table addressing)
```
[31:28] = opcode upper nibble (from base)
[27:14] = immediate_1 (14-bit)
[13:0]  = immediate_2 (14-bit)
```
Children: 2 — [0]=high immediate, [1]=low immediate

## Operand types accepted by the assembler

The assembler accepts three operand kinds in `.seqasm` source:

- **Register** — validated against the device's `registerDepth`.
- **Label / symbol** — looked up in the label bimap; the resolved
  integer is masked to fit the operand's bit width.
- **Integer literal** — checked against `(1 << bits) - 1` for
  overflow.

## Diagnostics

Encoding-time errors are surfaced through the assembler's error
catalogue; the most common are wrong argument count, register where
an immediate was expected (and vice versa), value overflow against
the operand width, undefined label, and memory-address out of range
for `ST`.

## See also

- \ref notes_opcode_map — complete opcode → mnemonic table with
  scheduling class.
- \ref notes_fb_instruction — the feedback (`fb`) instruction.
- \ref notes_special_registers — addresses targeted by `LD`/`ST`/
  `luser` / `suser`.

