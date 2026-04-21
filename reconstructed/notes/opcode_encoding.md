# Opcode Encoding Formats — AWGAssemblerImpl

## Summary of Instruction Word Formats (32-bit)

All opcodes produce a 32-bit instruction word.

### opcode0 — Zero-argument (NOP, END, etc.)
```
[31:0] = opcode_base (unchanged)
```
Children: 0 expected. Error if any children present.

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
Special: opcode==1 → literal 0x40000000 (internal encoding trick).
Special: 0x30000001 (WVFS_H) uses child[0] as 1-bit selector for upper byte.

### opcode4 — Complex dispatch (branch/load/store/trap)
Variable format depending on opcode and child count:

**0 children** (opcode is self-contained):
- 0xF0000000 (WPRF), 0xF1000000 (WWVF), 0xF7000000 (TRAP), 0xF8000000 (IRPT), 0xFF000000 (FB)

**1 child** (immediate only):
- 0xFE000000 (JMP): `[19:0] = val(20)` → `val | 0xFE000000`
- 0xFD000000 (WTRIGI): `[4:0] = val(5)` → `val | 0xFD000000`  
- Others: `val(20) | opcode_base` or `val(24) | opcode_base`

**2 children** (register + immediate):
- 0xF6000000 (ST): `reg<<20 | val(20) | 0xF6000000` (with memory bounds check)
- 0xD0-D2 (LD/WTRIG/etc): `reg<<20 | val(20) | opcode_base`
- Others: `reg<<20 | val(20) | opcode_base`

### opcode5 — 2×14-bit Immediates (waveform table addressing)
```
[31:28] = opcode upper nibble (from base)
[27:14] = immediate_1 (14-bit)
[13:0]  = immediate_2 (14-bit)
```
Children: 2 — [0]=high immediate, [1]=low immediate

---

## AsmExpression Interface (Reconstructed)

```cpp
struct AsmExpression {
    int type;                    // +0x00: 1=register, 2=label/symbol, 3=integer literal
    std::string name;            // +0x08: symbol/label name (used when type==2)
    Assembler::Command command;  // +0x38: the command enum for the instruction
    int value;                   // +0x3C: register number (type==1) or immediate (type==3)
    std::vector<std::shared_ptr<AsmExpression>> children;  // +0x40: operands
};
```

### Type semantics:
- **type 1** (Register): `value` = register index. Validated against `deviceConstants->registerDepth`.
- **type 2** (Label/Symbol): `name` = label string. Looked up in `labelBimap` (at `this+0xD8`, string↔int bimap). The resolved integer is masked to fit the bit width.
- **type 3** (Integer literal): `value` = the literal integer. Checked against `(1<<bits)-1` overflow.

### Error Message IDs used:
- 1: "expected register at argument position X of opcode Y"
- 2: "expected value at argument position X of opcode Y"  
- 3: "invalid register" (negative)
- 4: "wrong number of arguments" (with command name, expected, got)
- 5: "value overflow" (value, max_bits)
- 6: "wrong format for register-register operation"
- 7: "wrong argument count" (command_name, opcode_type, expected)
- 8: "expected register expression"
- 9: "expected value expression (integer or label)"
- 10: "memory address out of range" (max_depth)
- 0x78: "undefined label" (label_name)

### DeviceConstants offsets used:
- `+0x28`: `registerDepth` (uint64_t) — max register count
- `+0x30`: `memoryDepth` (uint64_t) — max memory address for ST bounds check

### AWGAssemblerImpl layout (relevant offsets):
- `+0x00`: pointer to DeviceConstants
- `+0xD8`: label bimap (boost::bimaps::bimap<string, int>)
