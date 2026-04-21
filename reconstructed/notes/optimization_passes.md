# AsmOptimize — Optimization Passes

Reconstructed from Phase 6 disassembly. AsmOptimize operates on a working
copy of the AsmList (vector<AsmList::Asm>). Passes are selected by a
bitmask at +0x08 (flags_).

## Pass Pipeline

### Pre-waveform (`optimizePreWaveform`, 0x27db40)

Called before waveform allocation. Only one pass:

1. **deadCodeElimination** (flag 0x04)

### Post-waveform (`optimizePostWaveform`, 0x27ddf0)

Called after waveform allocation. Runs passes in order:

1. **oneStepJumpElimination** (flag 0x01)
2. **removeUnusedLabels** + **mergeLabels** (flag 0x02)
3. **mergeRegisterZeroing** (flag 0x08)
4. **removeUnusedRegs** + **registerAllocation** (flag 0x10)
5. **reportUserMessages** (always, no flag check)

On registerAllocation failure, the catch block:
- Restores the pre-allocation asm list from a backup copy
- Calls `splitConstRegisters(numRegs)` to reduce register pressure
- Retries `registerAllocation(newNumRegs)`

## Pass Details

### deadCodeElimination (0x27dbd0)

Walks the instruction list linearly. After encountering an unconditional
branch (JMP, or BRZ with dest==r0), marks subsequent instructions as dead
(`cmd = 0xFFFFFFFF`) until a *referenced* label is found. Unreferenced
labels in the dead region are also killed. Dead instructions have their
Node references removed via `Node::remove()`.

### oneStepJumpElimination (0x27e040)

For each branch/jump instruction, scans forward skipping dead/NOP
instructions. If the first reachable non-dead instruction is a LABEL
matching the branch target, the branch is eliminated (redundant jump to
next instruction).

Respects the `noOpt` flag on AsmList::Asm entries — instructions marked
noOpt are skipped.

### removeUnusedLabels (0x27e1c0)

For each LABEL instruction, scans the entire list for any BRZ/BRNZ/BRGZ/JMP
referencing that label. If no reference found, marks the label as dead and
clears its label string.

### mergeLabels (0x27e330)

When consecutive LABEL instructions exist, replaces all references to the
second label with the first label, then marks the second as dead. Handles
chains of consecutive labels (iterates the `next` pointer).

### mergeRegisterZeroing (0x27e640)

Pattern: `ADDI rN, r0, 0` followed by `ADDR rN, rN, rM` where the ADDR
dest == reg1 and matches the ADDI's reg0. The ADDI is redundant (the ADDR
already zeroes). Marks ADDI as dead and sets ADDR's dest to r0.

Note: The binary checks cmd == 0x50000000 (ADDR), not XORR. The register
zeroing pattern uses ADDR (add-register), not XOR.

### removeUnusedRegs (0x27e760)

Complex pass (~400 asm lines, approximate reconstruction):
1. Tracks highest register number seen (returned to caller)
2. If flag 0x08 is set, identifies write-only registers (written but never
   subsequently read)
3. Uses `getNextActionForReg` to determine read/write status
4. May call `simplifyAssign` for ADDI+XORR patterns
5. Eliminates write-only register instructions

Also checks cancellation via the shared_ptr<CancelCallback> at +0x90.

### reportUserMessages (0x280b60)

Extracts MESSAGE (cmd==5) and ERROR_MSG (cmd==3) pseudo-instructions.
For each:
- Converts the first immediate operand to a string
- Reads the line number from AsmList::Asm.lineNumber (+0x88)
- Calls `errorCallback_` (for ERROR_MSG) or `warningCallback_` (for MESSAGE)
  via the std::function objects at +0x30 and +0x60
- If flags_ is non-zero, marks the instruction as dead

The callbacks are `std::function<void(const std::string&, int)>` — they
receive the message text and source line number.

### simplifyAssign (0x280e10)

Helper for removeUnusedRegs. If the next instruction after `it` is
`ADDI dest, src, 0` (copy via add-zero) and the dest register is not
used later, the copy can be elided by redirecting the source directly.

## Register Allocator

### registerAllocation (0x27ebb0)

~1900 lines of assembly. Graph-coloring register allocator:

1. **Build live ranges**: For each virtual register (>0), collect all
   instruction indices where it appears in any register slot (dest, reg0,
   reg1, reg2). Stored as `vector<vector<int>>` indexed by register number.

2. **Build conflict graph**: Two virtual registers conflict if their live
   ranges share any instruction index. Tracked via `std::set<unsigned long>`.

3. **Greedy allocation**: For each virtual register, try to assign it to a
   physical register (0..numPhysicalRegs_) that has no conflicting live
   range overlap. Physical registers tracked as `vector<vector<int>>`.

4. **On success**: Call `registerUpdate` to replace all occurrences of the
   virtual register with the assigned physical register.

5. **On failure**: Throw `OptimizeException`, caught by `optimizePostWaveform`
   which retries with `splitConstRegisters`.

Checks cancellation periodically via CancelCallback.

### splitConstRegisters (0x280440)

~1800 bytes. Reduces register pressure by splitting constant-loaded registers:

1. Find `ADDI rN, r0, #imm` instructions (constant loads from r0)
2. If rN has a long live range crossing many instructions, split it by
   inserting new `ADDI rNew, r0, #imm` instructions closer to use sites
3. Fresh virtual register numbers allocated from `GlobalResources::regNumber` (TLS)
4. Returns the new maximum register number

### splitReg (0x281000)

Splits a register's live range at given boundaries:
1. Allocates a fresh virtual register from `GlobalResources::regNumber`
2. Inserts a copy instruction (`ADDI newReg, oldReg, 0`) at the split point
3. Replaces all occurrences of oldReg with newReg in [start, end)

### registerUpdate (0x281680)

Simple replacement pass: iterates the given instruction indices (in reverse)
and replaces all occurrences of oldReg with newReg in dest, reg1, and reg2
slots.

## Query Helpers

### isRead (0x27d900)

Checks if an instruction reads a given register:
- reg0 (+0x20): read if `getCmdType(cmd) & 1` (bit 0 set)
- reg1 (+0x30): read if cmdType == 7 or cmdType == 1

### isWritten (0x27d960)

Checks if an instruction writes a given register:
- dest (+0x28): written if `(getCmdType(cmd) >> 1) & 1` (bit 1 set)
- reg1 (+0x30): written if cmdType == 7

### getCmdType bitmask semantics

| cmdType | Meaning          | Reads reg0 | Writes dest | reg1 role |
|---------|------------------|------------|-------------|-----------|
| 0       | no-reg           | no         | no          | —         |
| 1       | read-only        | yes        | no          | read      |
| 2       | write-only       | no         | yes         | —         |
| 3       | read+write       | yes        | yes         | —         |
| 7       | read+write+reg1  | yes        | yes         | read+write|

### isLabelCalled (0x27d9c0)

Scans the entire asm list for BRZ/BRNZ/BRGZ/JMP instructions whose label
matches the given label string.

### getNextActionForReg (0x281a10)

Scans forward, returns a bitmask: bit 0 = read, bit 1 = written.
Returns 3 immediately on branch commands or when the register appears in
all slots. Returns 0 at end of list.

### registerIsNeverWritten (0x280f50)

Scans [start+1, end) skipping `exclude`, returns false if any instruction
writes the register (same write semantics as isWritten).

## Global State

- `GlobalResources::regNumber` — thread-local counter for allocating fresh
  virtual register numbers. Incremented by `prepareResources` (to exceed
  the existing maxRegister) and by `splitReg` / `splitConstRegisters`.
