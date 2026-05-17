# AsmOptimize — Optimization Passes {#notes_optimization_passes}

`AsmOptimize` operates on a working copy of the `AsmList`
(`vector<AsmList::Asm>`).  Passes are selected by a bitmask
(`optFlags_`) that the compiler sets from its configuration.

## Pass Pipeline

### Pre-waveform (`optimizePreWaveform`)

Called before waveform allocation.  Only one pass:

1. **deadCodeElimination** (flag 0x04)

### Post-waveform (`optimizePostWaveform`)

Called after waveform allocation.  Runs passes in order:

1. **oneStepJumpElimination** (flag 0x01)
2. **removeUnusedLabels** + **mergeLabels** (flag 0x02)
3. **mergeRegisterZeroing** (flag 0x08)
4. **removeUnusedRegs** + **registerAllocation** (flag 0x10)
5. **reportUserMessages** (always, no flag check)

On `registerAllocation` failure, the catch block:

- Restores the pre-allocation asm list from a backup copy.
- Calls `splitConstRegisters(numRegs)` to reduce register pressure.
- Retries `registerAllocation(newNumRegs)`.

## Pass Details

### deadCodeElimination

Walks the instruction list linearly.  After encountering an
unconditional branch (`JMP`, or `BRZ` with `dest==r0`), marks
subsequent instructions as dead (`cmd = 0xFFFFFFFF`) until a
*referenced* label is found.  Unreferenced labels in the dead
region are also killed.  Dead instructions have their `Node`
references removed via `Node::remove()`.

### oneStepJumpElimination

For each branch / jump instruction, scans forward skipping
dead / NOP instructions.  If the first reachable non-dead
instruction is a `LABEL` matching the branch target, the
branch is eliminated (redundant jump to next instruction).

Respects the `noOpt` flag on `AsmList::Asm` entries — instructions
marked `noOpt` are skipped.

### removeUnusedLabels

For each `LABEL` instruction, scans the entire list for any
`BRZ` / `BRNZ` / `BRGZ` / `JMP` referencing that label.  If no
reference is found, marks the label as dead and clears its
label string.

### mergeLabels

When consecutive `LABEL` instructions exist, replaces all
references to the second label with the first label, then
marks the second as dead.  Handles chains of consecutive
labels (iterates the `next` pointer).

### mergeRegisterZeroing

Pattern: `ADDI rN, r0, 0` followed by `ADDR rN, rN, rM` where
the `ADDR` dest equals `regAux` and matches the `ADDI`'s
`regDst`.  The `ADDI` is redundant (the `ADDR` already zeroes).
Marks `ADDI` as dead and sets `ADDR`'s dest to `r0`.

Note: the binary checks `cmd == 0x50000000` (`ADDR`), not `XORR`.
The register-zeroing pattern uses `ADDR` (add-register), not
`XOR`.

### removeUnusedRegs

Complex pass:

1. Tracks the highest register number seen (returned to caller).
2. If flag `0x08` is set, identifies write-only registers
   (written but never subsequently read).
3. Uses `getNextActionForReg` to determine read / write status.
4. May call `simplifyAssign` for `ADDI` + `XORR` patterns.
5. Eliminates write-only register instructions.

Also checks cancellation via the shared cancel callback.

### reportUserMessages

Extracts `MESSAGE` (cmd==5) and `ERROR_MSG` (cmd==3)
pseudo-instructions.  For each:

- Converts the first immediate operand to a string.
- Reads the line number from `AsmList::Asm.lineNumber`.
- Calls `errorCallback_` (for `ERROR_MSG`) or `warningCallback_`
  (for `MESSAGE`).
- If `optFlags_` is non-zero, marks the instruction as dead.

The callbacks are `std::function<void(const std::string&, int)>` —
they receive the message text and source line number.

### simplifyAssign

Copy-propagation helper for `removeUnusedRegs`.  Checks if the
instruction at `it` writes to register `rX` (via its write-dest
slot), and the next instruction is `ADDI rY, rX, 0` (read-source
`rX`, write-dest `rY`, sole `outputs[0]` immediate zero).  If
no subsequent instruction reads `rX` via its read-source or
auxiliary register slots, eliminates the copy by setting
`it->regDst = next->regDst` (redirect the write to `rY`
directly) and marking the `ADDI` as `INVALID`.

Key detail: the zero immediate is checked via the
`outputs[0]` vector, **not** via the `immediates` vector.  The
forward read scan starts at `it + 2`, not `it + 1`.

## Register Allocator

### registerAllocation

Graph-colouring register allocator.

1. **Build live ranges**: for each virtual register (>0), collect
   all instruction indices where it appears in any register slot
   (dest, regDst, regAux, regSrc).  Stored as
   `vector<vector<int>>` indexed by register number.
2. **Build conflict graph**: two virtual registers conflict if
   their live ranges share any instruction index.  Tracked via
   `std::set<unsigned long>`.
3. **Greedy allocation**: for each virtual register, try to assign
   it to a physical register (`0..numPhysicalRegs_`) that has no
   conflicting live-range overlap.  Physical registers are
   tracked as `vector<vector<int>>`.
4. **On success**: call `registerUpdate` to replace all
   occurrences of the virtual register with the assigned
   physical register.
5. **On failure**: throw `OptimizeException`, caught by
   `optimizePostWaveform`, which retries with
   `splitConstRegisters`.

Checks cancellation periodically via the cancel callback.

### splitConstRegisters

Reduces register pressure by splitting constant-loaded registers.

1. Find `ADDI rN, r0, #imm` instructions (constant loads from
   `r0`).
2. If `rN` has a long live range crossing many instructions,
   split it by inserting new `ADDI rNew, r0, #imm` instructions
   closer to use sites.
3. Fresh virtual register numbers are allocated from
   `GlobalResources::regNumber` (thread-local).
4. Returns the new maximum register number.

### splitReg

Live-range splitter.

Algorithm:

1. Iterates instructions in `(start, end]` (i.e. `start + 1`
   through `list.end()`), skipping `INVALID` / `LABEL` /
   `cmd == 4` via a small skip bitmask (see below).
2. For each instruction, checks if `reg` is **read** (i.e. it
   appears in the read-source slot when `cmdType & 1`, or in
   the auxiliary slot when `cmdType ∈ {1, 7}`).  If the
   instruction both reads **and** writes `reg` (auxiliary slot
   with `cmdType == 7`), it is skipped — splitting is not safe.
3. Counts matching instructions.  Only triggers a split when the
   count reaches ≥ 10 (the threshold for worthwhile splitting).
4. On the first qualifying match: allocates a fresh virtual
   register from `GlobalResources::regNumber`.
5. Overwrites the `Asm` entry at `start` with a copy instruction
   (`ADDI newReg, reg, 0`) cloned from a nearby instruction's
   assembler.
6. If `end != list.end()`, overwrites the `Asm` entry at `end`
   similarly.
7. Replaces `regDst` / `regAux` references to `reg` with
   `newReg` in the current instruction (read-source slots are
   left untouched).
8. After the loop: if every split succeeded and at least one
   split occurred, kills the original boundary instructions by
   setting `cmd = INVALID` (`0xFFFFFFFF`).

### registerUpdate

Simple replacement pass: iterates the given instruction indices
(in reverse) and replaces all occurrences of `oldReg` with
`newReg` in the destination, auxiliary, and source register
slots.

## Query Helpers

### isRead

Checks if an instruction reads a given register:

- read-source slot: read if `getCmdType(cmd) & 1` (bit 0 set);
- auxiliary slot: read if `cmdType == 7` or `cmdType == 1`.

### isWritten

Checks if an instruction writes a given register:

- destination slot: written if `(getCmdType(cmd) >> 1) & 1`
  (bit 1 set);
- auxiliary slot: written if `cmdType == 7`.

### getCmdType bitmask semantics

| cmdType | Meaning            | Reads regDst | Writes dest | regAux role  |
|---------|--------------------|--------------|-------------|--------------|
| 0       | no-reg             | no           | no          | —            |
| 1       | read-only          | yes          | no          | read         |
| 2       | write-only         | no           | yes         | —            |
| 3       | read+write         | yes          | yes         | —            |
| 7       | read+write+regAux  | yes          | yes         | read+write   |

### isLabelCalled

Scans the entire asm list for `BRZ` / `BRNZ` / `BRGZ` / `JMP`
instructions whose label matches the given label string.

### getNextActionForReg

Scans forward, returns a bitmask: bit 0 = read, bit 1 = written.
Returns `3` immediately on branch commands or when the register
appears in all slots.  Returns `0` at end of list.

### registerIsNeverWritten

Scans `[start + 1, end)` skipping `exclude`, returns `false` if
any instruction writes the register (same write semantics as
`isWritten`).

## Skip bitmask 0x29 pattern

Several `AsmOptimize` functions use:

```
(cmd + 1) <= 5 && ((0x29 >> (cmd + 1)) & 1)
```

Bitmask `0x29` = bits 0, 3, 5 → skips cmd values: `INVALID`
(`-1`), `LABEL` (`2`), and `cmd == 4` (an undocumented value
between `MESSAGE = 3` and `ERROR_MSG = 5`).

## Global State

- **`GlobalResources::regNumber`** — thread-local counter for
  allocating fresh virtual register numbers.  Incremented by
  `prepareResources` (to exceed the existing `maxRegister`) and
  by `splitReg` / `splitConstRegisters`.

## See also

- \ref notes_magic_numbers_proposal — the `optFlags_` bitmask,
  `getCmdType` return values, and other unnamed constants this
  page references.
- \ref notes_pipeline — where these passes sit in the compiler
  pipeline.
