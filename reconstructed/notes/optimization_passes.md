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
dest == regAux and matches the ADDI's regDst. The ADDI is redundant (the ADDR
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

Copy-propagation helper for removeUnusedRegs. Checks if the instruction
at `it` writes to register `rX` (via regDst/+0x28, write-dest), and the
NEXT instruction is `ADDI rY, rX, 0` (regSrc=rX, regDst=rY, outputs[0]=0).
If no subsequent instruction reads `rX` via regSrc or regAux, eliminates the
copy by setting `it->regDst = next->regDst` (redirect write to `rY` directly)
and marking the ADDI as INVALID.

**Key detail**: checks `outputs[0]` (vector at +0x38), NOT `immediates`
(vector at +0x08) for the zero-immediate. Also checks regSrc (read-src)
matches it->regDst (write-dest), and scans from `it+2` (not `it+1`).

## Register Allocator

### registerAllocation (0x27ebb0)

~1900 lines of assembly. Graph-coloring register allocator:

1. **Build live ranges**: For each virtual register (>0), collect all
   instruction indices where it appears in any register slot (dest, regDst,
   regAux, regSrc). Stored as `vector<vector<int>>` indexed by register number.

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

Live-range splitter (~500 lines binary, fully reconstructed in Phase 21c).

Algorithm:
1. Iterates instructions in `(start, end]` (start+1 through list.end()),
   skipping INVALID/LABEL/cmd4 via the `0x29` bitmask on `(cmd+1)`.
2. For each instruction, checks if `reg` is READ (regDst with cmdType&1,
   or regAux with cmdType∈{1,7}). If the instruction both reads AND writes
   `reg` (regAux with cmdType==7), it's skipped (not safe to split).
3. Counts matching instructions. Only triggers a split when count ≥ 10
   (threshold for worthwhile splitting).
4. On first qualifying match: allocates a fresh virtual register from
   `GlobalResources::regNumber` (thread-local int at TLS+0x48).
5. Overwrites the Asm entry at `start` with a copy instruction
   (ADDI newReg, reg, 0) cloned from a nearby instruction's assembler.
6. If `end != list.end()`, overwrites the Asm entry at `end` similarly.
7. Replaces regDst/regAux references to `reg` with `newReg` in the current
   instruction.
8. After loop: if all splits succeeded (allSplitOk=true) AND at least
   one split occurred, kills the original boundary instructions by
   setting cmd = INVALID (0xFFFFFFFF).

Key corrections from the 20-line stub (Phase 21c):
- Was missing the ≥10 threshold, boundary Asm overwrites, allSplitOk/
  didSplit flags, and the post-loop kill logic.
- Was doing a simple 4-field replace loop; binary only replaces regDst/regAux
  (not regSrc) and only for instructions that read the register.

### registerUpdate (0x281680)

Simple replacement pass: iterates the given instruction indices (in reverse)
and replaces all occurrences of oldReg with newReg in dest, regAux, and regSrc
slots.

## Query Helpers

### isRead (0x27d900)

Checks if an instruction reads a given register:
- regDst (+0x20): read if `getCmdType(cmd) & 1` (bit 0 set)
- regAux (+0x30): read if cmdType == 7 or cmdType == 1

### isWritten (0x27d960)

Checks if an instruction writes a given register:
- dest (+0x28): written if `(getCmdType(cmd) >> 1) & 1` (bit 1 set)
- regAux (+0x30): written if cmdType == 7

### getCmdType bitmask semantics

| cmdType | Meaning          | Reads regDst | Writes dest | regAux role |
|---------|------------------|------------|-------------|-----------|
| 0       | no-reg           | no         | no          | —         |
| 1       | read-only        | yes        | no          | read      |
| 2       | write-only       | no         | yes         | —         |
| 3       | read+write       | yes        | yes         | —         |
| 7       | read+write+regAux  | yes        | yes         | read+write|

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

---

# Register field semantics — critical correction (Phase 15c, 2026-04-23)

`AssemblerInstr` register field semantics were **inverted** in early
reconstruction and propagated through all AsmOptimize methods. Phase 15c
discovered and corrected this.

## Correct semantics (from disassembly of `isRead` @0x27d900 and `isWritten` @0x27d960)

| Field | AssemblerInstr offset | Asm offset | Semantic |
|-------|----------------------|------------|----------|
| regSrc  | +0x20                | +0x28      | **READ source** (isRead checks with cmdType & 1) |
| regDst  | +0x28                | +0x30      | **WRITE destination** (isWritten checks cmdType bit 1) |
| regAux  | +0x30                | +0x38      | **Dual**: read if cmdType==1 or 7; written if cmdType==7 |

Prior code had regSrc as "dest" and regDst as "src1" — exactly backwards.
The field **names** in `assembler.hpp` were NOT changed (to avoid
cascading rename across 20+ files), but all **semantic usage** in
asm_optimize.cpp was corrected with offset citations in comments.

## Functions corrected by Phase 15c

| Function | Address | Fix |
|----------|---------|-----|
| `isRead` | 0x27d900 | Checks regSrc(+0x20), not regDst |
| `isWritten` | 0x27d960 | Checks regDst(+0x28), not regSrc |
| `getNextActionForReg` | 0x281a10 | Rewritten: correct 3-field scan (regSrc→bit0, regDst→bit1, regAux→3); branch commands return 3 early |
| `registerIsNeverWritten` | 0x280f50 | Field references corrected |
| `registerUpdate` | 0x281680 | Updates all 3 register fields correctly |
| `isLabelCalled` | 0x27d9c0 | Iteration direction fixed: begin..it (not it..end) |

## Functions still using pre-correction field references (carry-forward)

- `simplifyAssign` @0x280e10 — not in scope of Phase 15c. Should be
  revisited if this function is ever exercised by tests.
- `splitReg` @0x281000 — current stub is ~20 lines; real binary is ~500.

---

# Algorithm reconstructions (Phase 15c)

## `removeUnusedRegs` @0x27e760 (291 lines, fully reconstructed)

Algorithm:
1. Cancel-callback lock pattern at entry.
2. Skip bitmask 0x29 = 0b101001: skips INVALID(-1), LABEL(2), and cmd=4
   (NOT NOP/MESSAGE/ERROR_MSG as previously claimed).
3. For each writing instruction: determine dest reg (try regDst, fallback
   regAux for cmdType==7).
4. Track seen dest regs in vector to avoid re-processing.
5. Inner forward scan builds bitmask: bit0=reg in regSrc (read),
   bit1=reg in regDst (overwritten); regAux match or both bits → abandon.
6. After scan:
   - never-read → eliminate (cmd=INVALID, dest=AsmRegister(-1))
   - read-but-not-overwritten → call `simplifyAssign`

## `registerAllocation` @0x27ebb0 (1466 lines, structurally reconstructed)

6-phase graph-coloring:

1. Build live ranges: `vector<vector<int>>` (0x18 per slot), one per
   unique register.
2. Build label→index map: `unordered_map<string,int>`.
3. Find backward branches: pairs of (branch-index, label-index) where
   label precedes branch.
4. Extend live ranges across backward branches.
5. Allocate physical registers: `set<unsigned long>` for conflicts,
   greedy assignment.
6. Rename via `registerUpdate`.

Internal type `PhysicalRegister` is confirmed by the vector destructor
mangled name at 0x281840.

## `splitConstRegisters` @0x280440 (444 lines, structurally reconstructed)

Two-pass algorithm:

- **Pass 1**: Build rewritten instruction list with "barrier" entries
  (cmd=INVALID, dest=`magicSkipRegister()`).
- **Pass 2**: Scan for constant loads (regDst==r0) and call `splitReg` to
  split live ranges.
- **Post-pass**: Move entries back to asm_, stripping barrier entries.

## Skip bitmask 0x29 pattern

Multiple AsmOptimize functions use:

```
(cmd+1) <= 5 && ((0x29 >> (cmd+1)) & 1)
```

Bitmask 0x29 = bits 0,3,5 → skips cmd values: INVALID(-1), LABEL(2),
and cmd=4 (undocumented value between MESSAGE=3 and ERROR_MSG=5). This
pattern was previously incorrectly claimed to skip NOP/MESSAGE/ERROR_MSG.

## `AsmRegister::magicSkipRegister()` @0x28ebb0

Returns `{INT_MAX, true}` (packed as `0x17fffffff`). Used by
`splitConstRegisters` to mark barrier entries for later stripping.
