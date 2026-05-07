# splitReg Loop Model — GDB-Verified Reconstruction of 0x281000..0x2814cc

Verified for `hb_b_reg_count.seqc` (HDAWG8). Counters and per-iter
behavior captured by GDB on the original `_seqc_compiler.so` binary.

## Function signature

```cpp
void AsmOptimize::splitReg(AsmList& list,
                           AsmRegister reg,
                           AsmList::const_iterator start,
                           AsmList::const_iterator end);
```

`start` and `end` are iterators into `list`. The loop walks `(start, list.end())`
— **NOT** `(start, end)`. `end` is used only as a clone source for Block 2 and
as a gating value (`end != list.end()`).

## Stack layout (essentials)

| addr      | meaning                                            |
|-----------|----------------------------------------------------|
| `[rbp-0x60]` | list.end() (saved at entry)                     |
| `[rbp-0x58]` | end iterator (saved)                             |
| `[rbp-0x80]` | startOff = (start - list.begin) in bytes        |
| `[rbp-0x78]` | endOff   = (end - list.begin) in bytes          |
| `[rbp-0x90]` | TLS pointer to `GlobalResources::regNumber`     |
| `[rbp-0x50]` | TLS pointer to `Asm::createUniqueID` counter+0x40|
| `[rbp-0x98]` | `&start->assembler` (start.assembler base addr) |
| `[rbp-0x88]` | `&end->assembler`                                |
| `[rbp-0x48]` | newReg (8 bytes, AsmRegister)                   |
| `[rbp-0x40]` | reg (8 bytes, the parameter)                    |
| `[rbp-0x70]` | `&current->assembler.regSrc` (= r12+0x28)       |
| `[rbp-0x68]` | `&current->assembler.regAux` (= r12+0x38)       |
| `[rbp-0x30]` | bool didSplit (initially 0)                     |
| `[rbp-0x34]` | bool allSplitOk (initially 1)                   |
| `[rbp-0x2c]` | tentative new r14d (= r14d_old + 1)             |

## Loop variables

- `r12` = pointer to current Asm. Initialized to `start + 0xa8` (= `&start[1]`).
  Advances by `0xa8` per iteration in loop tail.
- `r14d` = "non-skip iteration count" (call it `instrCount`). Initialized to 0.
  Incremented by 1 on every non-skip iter. **NEVER reset** — even after a split.

## Per-iteration logic

```
1. Skip-check: cmdPlus1 = cmd+1; if cmdPlus1 in {0,3,5} (= cmd in {INVALID, LABEL, cmd4}): SKIP (do not bump r14d, jmp loop tail without [rbp-0x2c] write — i.e., 2810b0 path).
   Actually the skip BACK-jumps to 2810b0 which goes to: `mov eax,r14d; r12+=0xa8; mov r14d,eax; cmp; jmp/loop`. So r14d preserved.

2. Tentative bump: [rbp-0x2c] = r14d + 1.

3. cmdType = getCmdType(cmd).

4. Reads-reg detection (from disasm 2810e7..28116f):
   - case A: regSrc==reg AND cmdType&1 (instruction reads regSrc):
       - if regDst==reg AND (cmdType>>1)&1 (instruction also writes regDst): ABANDON
       - elif regAux==reg AND cmdType==7 (instruction also writes regAux): ABANDON
       - else: USE-DETECTED → goto SPLIT_CHECK
   - case B: !A AND regAux==reg:
       - if cmdType in {1,7}: re-enter case A's WRITES-CHECK (jump back to 281100)
       - else: ABANDON
   - else: ABANDON

5. SPLIT_CHECK: if r14d < 10 → set allSplitOk=false, ABANDON.
   Else → SPLIT_BUMP.

6. ABANDON path: jmp 281470 → loop tail → r14d := [rbp-0x2c] = r14d_old + 1.

7. SPLIT_BUMP:
   a. newRegNum = *regNumberPtr; (*regNumberPtr)++; newReg = AsmRegister(newRegNum).
      ** POST-INCREMENT semantics: newReg gets the OLD value, ptr is bumped.
   b. seqId1 = *createUniqueIDPtr; (*createUniqueIDPtr)++.
   c. BLOCK 1 — write to slot at &Asm[iter-2] (= r12 - 0x150 = r12 - 2*sizeof(Asm)):
      - sequenceId = seqId1
      - assembler = clone of start->assembler (via Assembler copy ctor)
        then PATCH: assembler.regDst = newReg
        (cmd, regSrc, regAux, immediate, etc. all UNCHANGED from start)
      - wavetableFront = current->wavetableFront (loaded from r12+0x88 just before)
      - noOpt = ((start->assembler.cmd - 3) < 3u)
      - node = nullptr (zeroed; the slot's existing node is released first)
   d. If [rbp-0x58] != [rbp-0x60]  (end != list.end()):
      seqId2 = *createUniqueIDPtr; (*createUniqueIDPtr)++.
      BLOCK 2 — write to slot at &Asm[iter-1] (= r12 - 0xa8):
      - sequenceId = seqId2
      - assembler = clone of end->assembler
        then PATCH: assembler.regDst = newReg
        and PATCH: assembler.regSrc = (start->assembler.cmd == ADDI(0x40000000) ? newReg : AsmRegister(0))
      - wavetableFront = current->wavetableFront (re-loaded)
      - noOpt = ((end->assembler.cmd - 3) < 3u)
      - node = nullptr
   e. Rename CURRENT instruction (at r12):
      - if (current->assembler.regSrc == reg) current->assembler.regSrc = newReg
      - if (current->assembler.regAux == reg) current->assembler.regAux = newReg
      ** NOT regDst! Renames regSrc and regAux only. **
   f. didSplit = true.

8. Loop tail (281470..28148f):
   - r14d := [rbp-0x2c]    (so non-skip iter advances r14d by exactly 1, including
                            on SPLIT iters)
   - r12 += 0xa8
   - if r12 != list.end() → next iter
   - else → epilogue

9. Epilogue (28148c..2814bb):
   - if (allSplitOk & didSplit):
       - list[startOff].assembler.cmd = INVALID (-1)
       - if end != list.end(): list[endOff].assembler.cmd = INVALID
   - return.
```

## Why splits self-throttle (3 per call observed)

After a split fires on instruction at iter K, the regSrc/regAux of that
instruction are renamed `reg → newReg`. Subsequent iters look for `reg`
again. Each future read-of-`reg` is one instruction whose regs haven't
been renamed yet. So the number of splits per call = number of
distinct read-uses of `reg` in the post-`start` portion of the list
that pass the threshold gate (r14d >= 10) AND aren't read+write.

For `hb_b_reg_count` with 16 active variables, each call fires exactly
3 splits because there are exactly 3 `wait(rN)` reads of each variable
positioned past the threshold in the lowered list.

## GDB trace observations

```
BUMP call=1 split#=1 r12=0x...75cae0 r14d=15
  BLK1 dest=0x...75c990   (= r12-0x150)
BUMP call=1 split#=2 r12=0x...75ea60 r14d=31
  BLK1 dest=0x...75e910
BUMP call=1 split#=3 r12=0x...762ab0 r14d=63
  BLK1 dest=0x...762960
EPI call=1 splits=3 allOk=1 didSplit=1
```

No BLK2 in this test (end == list.end() for every call).

## Recon bugs (pre-fix at asm_optimize_regalloc.cpp:666-782)

1. Computes ONE `startOff`/`endOff` and writes to those fixed slots every
   iter. Binary writes to per-iter slots `&list[iter-2]` and `&list[iter-1]`.
2. Block 1 clones from `end`. Binary clones from `start`.
3. Block 1 sets `cmd=ADDI`, `regSrc=reg`. Binary leaves `cmd` unchanged
   (whatever start has) and sets only `regDst = newReg`.
4. Block 2 unconditionally sets `regSrc = AsmRegister(0)`. Binary chooses
   based on `start.cmd == ADDI`.
5. Renames `regDst` + `regAux` of current. Binary renames `regSrc` + `regAux`.
6. Allocates `newReg` only on first split. Binary allocates fresh per split.
7. Missing: per-split `sequenceId` (1 or 2 fresh IDs from `createUniqueID`),
   `wavetableFront` propagation, `noOpt` recomputation, `node` reset.
8. (Epilogue logic in recon is essentially correct.)
