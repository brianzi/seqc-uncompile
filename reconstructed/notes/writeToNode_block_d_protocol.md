# writeToNode — Block D ([0x164b19..0x169d83]) emit-protocol catalogue

## Scope

Block D is the typeIdx switch (cases 0..5) reached after `oscselNodeRegex`
matches the resolved node string. Two entry paths feed the same six cases:

- **Path A** — fast path. dispatch `jmp rax` @ 0x164c7d via jt @ `0x958f68`.
  Reached when the `NodeMapItem` is found via `fastAddress()` (case 0x164c50)
  and `fastAddress()` returned a non-zero address. Each path-A case begins
  with a `cmp DWORD PTR [r12], 0x2` test on `val.varType_` (the visited
  variant index): the "fast" arm proceeds inline, the "slow" arm jumps to
  the variant-dispatch fallback that resolves through vtable @ `0xb03dc0`.
- **Path BC** — slow path. dispatch `jmp rcx` @ 0x164de2 via jt @ `0x958f50`.
  Reached when fastAddress returned 0 OR via `__hash_table::find` lookup
  in `r14->nodeMap` (lines 442..455). The resolved address is stored at
  `[rbp-0x17c]` immediately before the jump.

The two paths share a common register layout entering the case bodies
(`r14`=`this`, `r12`=`&val`, `[rbp-0x48]`=destReg AsmRegister, `[rbp-0x40]`
= localList vector, `[rbp-0x178/-0x170/-0x168]` = sret slot for addi()'s
returned `vector<Asm>`).

## "Emit step" pattern recap

Each emit step compiles to roughly the same template:

```
        mov    rbx, [r14+0x50]        ; rbx = &this->commands (AsmCommands&)
        mov    rXX, [rbp-0x48]        ; destReg
   (a)  lea    rdi, [rbp-XXX]         ; sret AsmRegister tmp
        xor    esi, esi
        call   AsmRegister(0)         ; tmp = R0
   (b)  lea    r12, [rbp-YYY]         ; sret Immediate tmp
        mov    esi, <imm-value>       ; or computed value
        call   Immediate::Immediate(int)
   (c)  ; build addi(rbx, R0=src1, destReg=src2, Immediate)  — sret @ rbp-0x178
        call   273d60 <AsmCommands::addi>
   (d)  ; insert returned vector<Asm> into localList @ rbp-0x40
        call   1266d0 <vector<Asm>::__insert_with_size>
   (e)  ; cleanup: ~AsmList(rbp-0x178), ~Immediate(rbp-YYY)
   (f)  ; suser step
        mov    rsi, [r14+0x50]
        mov    rdx, [rbp-0x48]
        mov    ecx, <opcode>          ; 0x10 / 0x11 / 0x12 / 0x17 / 0x19
        call   274bc0 <AsmCommands::suser>
        call   15d180 <AsmList::append>
        call   122dd0 <Asm::~Asm>
```

The Immediate sret slots walk down the frame by ~0x20..0x60 per step:
0x8d0, 0x8b0, 0x6b0, 0x670, 0x610, 0x5b0, 0xaa0, 0xa80, 0xa60, 0xa40,
0xa20, 0xa00, 0x9e0, 0x940, 0x6d0, 0x7d0, ... — each step reserves its
own non-overlapping AsmRegister and Immediate slot so cleanup at function
end has 1-1 destructors. This is why the frame is `0xad8` bytes.

## suser opcode meanings (inferred)

Counted occurrences in Block D:

| opcode | count | apparent meaning                                 |
|--------|-------|--------------------------------------------------|
| 0x10   | 11    | "user-store low / set"                            |
| 0x11   |  6    | "user-store mid / second component"              |
| 0x12   |  6    | "user-store high / third component"              |
| 0x17   |  9    | "direct-write opcode" (val.varType==2 fast arm)  |
| 0x19   |  4    | "direct-write opcode B" (companion to 0x17)      |

Cases that emit only 0x17 (and possibly 0x19) bypass the addi-chain because
the value is already a register passed directly via `[r12+0x30]` (the
variant payload pointer). Cases that emit 0x10/0x11/0x12 always precede
the suser by an `addi(R0, dst, Immediate)` that loads the literal value
into R0 first.

## Path-A case bodies

Path-A uses the variant fast-arm: the addi-chain runs only when
`val.varType_ == 2` (the "AsmRegister already" variant). When that test
fails the code branches into the per-step "boost::variant visitor"
fallback (variant indices stored at `[rbp-0x830]/-0x808]/...` selected
through vtable `@b03dc0`).

| typeIdx | start    | end (jmp to tail) | steps | per-step (immediate, suser opcode, variant-dispatch) | preconditions / extra calls |
|---------|----------|-------------------|-------|------------------------------------------------------|------------------------------|
| 0       | 0x164c7f | 0x164cb4 → 0x165c90 | 1 | (none, suser 0x17 only)                            | `cmp [r12],0x2; jne 0x165b17` (slow var-dispatch goes to addi-chain w/ `val.toInt()`); fast arm passes `val.payload=[r12+0x30]` directly to suser as the address |
| 1 (sine pair) | 0x165263 | 0x1652e1 → 0x165ffa | 2 | step1: (none, suser 0x17); step2: (none, suser 0x19) | No addi. `cmp [r12],0x2; jne 0x165d97` (slow arm). Fast arm directly emits two `suser` calls passing `[r12+0x30]` as address (opcode 0x17 then opcode 0x19) — the two halves of a stereo/sine command pair. |
| 2 (cvt to single-float) | 0x165013 | 0x1650d9 → 0x16729e | 1 | (`Immediate(float-bits of val.toDouble())`, 0x10) | Calls `Value::toDouble()` @0x165025; `cvtsd2ss xmm0,xmm0; movd esi,xmm0` to get bit-pattern. Slow arm @0x165013→0x16a191. |
| 3 (raw double-bits low32) | 0x165137 | 0x1651fc → 0x1674b2 | 1 | (`Immediate(low32 of val.toDouble() as int)`, 0x10) | Calls `Value::toDouble()` @0x165149; `movq rbx,xmm0` then truncate to ebx. Slow arm @0x165137→0x16a1f5. |
| 4 (freq)| 0x164ee7 | 0x164eec → 0x16a12d (or fallthrough chain) | 1 | (`Immediate(NodeMap::toFrequency(val.toDouble(), getSampleClock()))`, 0x10) | Calls `Value::toDouble()` @0x164ef9, `getSampleClock()` @0x164f06, `NodeMap::toFrequency(double,double)` @0x164f13. Slow-arm var-dispatch at 0x164ee7→0x16a12d. Body 0x164ef2..0x164fb4. |
| 5 (oscsel/phase) | 0x1652e6 | 0x1653a3 → 0x1676ce | 1 | (`Immediate(NodeMap::toPhase(val.toFloat()))`, 0x10) | Calls `Value::toDouble()` @0x1652f8; `cvtsd2ss; NodeMap::toPhase(float)` @0x165301. Slow arm @0x1652e6→0x16a259. |

NB: typeIdx ↔ semantic-name mapping corrected in Phase 22e (deferred cleanup).
Previous version had cases 1 and 4 swapped. The authoritative source is
`custom_functions_play.cpp` (writeToNode switch), confirmed against the
NodeTypeIdx enum in `node_map_data.hpp`:
{0:IntegerPassthrough, 1:SinePair, 2:FloatBits, 3:RawDoubleLow32,
4:Frequency, 5:Phase}.

## Path-BC case bodies

Path-BC has *no* val.varType==2 fast-arm precheck — the visitor variant
of `val` was already resolved upstream by the `dynamic_cast<DirectAddrNodeMapData*>`
flow. Each emit step uses `Immediate(int)` of either a literal small
constant (1,2,3,4,0xc,0xd) or `[rbp-0x17c]` (the resolved node address
that was stored just before `jmp rcx`). These cases *also* contain a
nested fast/slow arm on `val.varType_` but written differently from path
A: after the prologue they run a chain of 1-3 `addi/suser` pairs and
then a final `suser(opcode 0x12)` that closes the multi-word write.

| typeIdx | start    | first jmp → tail   | steps | per-step pattern                                                     | extra calls |
|---------|----------|--------------------|-------|----------------------------------------------------------------------|-------------|
| 0       | 0x164de4 | 0x164e7c → 0x166a3e | 1   | (`Immediate(1)`, no suser — just `addi(R0, dst, 1)`)                   | none        |
| 1       | 0x164ee7 | 0x164fb4 → 0x1678e2 | 1   | identical to path-A case 1 but reached without the var-dispatch path | `getSampleClock`, `NodeMap::toFrequency` |
| 2       | 0x165013 | 0x1650d9 → 0x16729e | 1   | `Immediate(float-bits)`, suser 0x10 (after the addi-chain)            | `Value::toDouble`, cvtsd2ss |
| 3       | 0x165137 | 0x1651fc → 0x1674b2 | 1   | `Immediate(low32(double))`, suser 0x10                                | `Value::toDouble` |
| 4       | 0x165488 | 0x165521 → 0x166e7e | 4   | step1: (`Immediate(2)`, 0x10); step2: (`Immediate(0xc)`, 0x10); step3: (`Immediate([rbp-0x17c])`, 0x11); step4: (no addi — direct suser 0x12 on `val`) | The triplet (low/mid/high) typical of 64-bit immediate split into 12+20+? bits. Slow arm: 0x165488→0x16a2bd. |
| 5       | 0x165587 | 0x165916 → 0x166865 | 4-5 | step1: (`Immediate(0xc)`, 0x10); step2: (`Immediate([rbp-0x17c])`, 0x11); step3: pure suser 0x12 with payload=`[rbx+0x30]`; (then a *second triplet* repeats: `Immediate(0xd)`, 0x10; `Immediate([rbp-0x17c])`, 0x11; suser 0x12) | Two consecutive low/mid/high triplets — this is the AWG sine "two halves" emit (matches sineNodeRegex preconditioning at 0x16492d). Outer slow arm: 0x165587→0x166107 (which itself runs the same chain with `Immediate(3)` instead of `0xc`/`0xd`). |

## Multi-step "reference" case at 0x165488 (path-BC typeIdx 4) — annotated

Lines 823..959 of the disasm. Three emit steps + one tail suser:

```
0x165488  cmp [r12],0x2 / je 0x16a2bd                         ; varType==2 slow-arm
0x165493  STEP 1: addi(R0, dst, Immediate(2)) ; sret @rbp-0x178; insert
0x165587  STEP 2: addi(R0, dst, Immediate(0xc))
0x165632  STEP 2 suser: opcode 0x10 ; append + ~Asm
0x165658  STEP 3: addi(R0, dst, Immediate([rbp-0x17c]=addr))  ; <-- the resolved addr goes through addi
0x165702  STEP 3 suser: opcode 0x11 ; append + ~Asm
0x165723  STEP 4: pure suser(rbx[0x30], opcode 0x12) ; append + ~Asm
0x16574c  jmp 0x16636b  (common tail = path-BC case 5's continuation)
```

Note that **step 3's addi-immediate is the node address**, and the
trailing `suser opcode 0x12` takes its address from `[rbx+0x30]` where
`rbx = val_pointer` — i.e., from the variant's resolved `AsmRegister`
payload. This is how a 1-register store gets turned into a 3-write
sequence: load constant tag (0x10), load address (0x11), commit data (0x12).

## Multi-step "double triplet" at 0x165587 (path-BC typeIdx 5)

Two back-to-back triplets — most likely the AWG sine I+Q write:

```
TRIPLET A (sine real half):
  0x165587 cmp [r12],0x2 / jne 0x166107       ; var-dispatch fallthrough
  0x165592 addi(R0, dst, Immediate(0xc)) ; STEP 1
  0x165632 suser(opcode 0x10)
  0x165658 addi(R0, dst, Immediate(addr)) ; STEP 2
  0x165702 suser(opcode 0x11)
  0x165723 suser(rbx+0x30, opcode 0x12) ; STEP 3 (no addi)
TRIPLET B (sine imag half):
  0x165751 cmp [r12],0x2 / jne 0x166537       ; second var-dispatch fallthrough
  0x16575c addi(R0, dst, Immediate(0xd)) ; STEP 4
  0x1657fc suser(opcode 0x10)
  0x165822 addi(R0, dst, Immediate(addr)) ; STEP 5
  0x1658c7 suser(opcode 0x11)
  0x1658ed suser(rbx+0x30, opcode 0x12) ; STEP 6
  0x165916 jmp 0x166865                  ; common tail
```

The literal `0xc` in triplet A vs `0xd` in triplet B is the user-tag
distinguishing the two halves (offset 0/1 within a stereo channel).

## Summary of suser-opcode and Immediate-literal census per case

| path/case  | distinct Immediates           | suser opcodes seq | total emit steps |
|------------|-------------------------------|-------------------|------------------|
| A.0        | (none — direct passthrough)   | 0x17              | 0 addi / 1 suser |
| A.1        | toFrequency(toDouble, sampleClk)| 0x10            | 1                |
| A.2        | float-bits(toDouble)          | 0x10              | 1                |
| A.3        | low32(toDouble bits)          | 0x10              | 1                |
| A.4        | (none)                        | 0x17, 0x19        | 0 addi / 2 suser |
| A.5        | toPhase(toFloat)              | 0x10              | 1                |
| BC.0       | 1                             | (none)            | 1 addi / 0 suser |
| BC.1       | toFrequency(...)              | 0x10              | 1                |
| BC.2       | float-bits                    | 0x10              | 1                |
| BC.3       | low32                         | 0x10              | 1                |
| BC.4       | 2, 0xc, addr                  | 0x10, 0x11, 0x12  | 3 addi / 3 suser |
| BC.5       | 0xc, addr, 0xd, addr          | 0x10,0x11,0x12, 0x10,0x11,0x12 | 6 addi+suser pairs (two triplets) |

Variant-dispatch (vtable @b03dc0) tail-handlers are reached via the
`jne <slow>` branches at the start of every fast-arm; they do *the same*
addi chain but with the immediate sourced from `val.toInt()` (calls
`floatEqual` first to validate the double is integer; throws
`CustomFunctionsValueException` with ErrorMessage 0x80 if not).
The slow-arm bodies live in 0x165b17..0x165cdb (A.0 fallback),
0x165d97..0x165f7d (A.4 fallback), 0x166107..0x1666a7 (BC.5 first
triplet variant fallback), 0x167afe..0x167dc7 (later step fallbacks),
0x168622..0x16927d (single-step var-dispatch helpers), all sharing the
same `Immediate(val.toInt())` pattern.

## Non-emit work referenced from the case bodies

| address    | call                                          | purpose                                     |
|------------|------------------------------------------------|---------------------------------------------|
| 0x164ef9   | `Value::toDouble()`                            | unwrap variant payload                      |
| 0x164f06   | `CustomFunctions::getSampleClock()`            | needed for toFrequency                      |
| 0x164f13   | `NodeMap::toFrequency(double, double)`         | freq-encoded immediate                      |
| 0x165301   | `NodeMap::toPhase(float)`                      | phase-encoded immediate                     |
| 0x165b39   | `floatEqual(double, double)`                   | guard before `toInt()`                      |
| 0x165b2b   | `Value::toInt()`                               | int-encoded immediate                       |
| 0x163d00   | `CustomFunctionsValueException::ctor`          | fast-fail when float→int rejected           |
| 0x16b8f0/0x106170 | `ErrorMessages::format<...>(0x80/0x84/0x85)` | format error string for the exception |
| 0x122dd0   | `AsmList::Asm::~Asm()`                         | per-step cleanup of single-element sret     |
| 0x11d5b0   | `AsmList::~AsmList()` (vector cleanup)         | per-step cleanup of multi-element sret      |
| 0x15c4f0   | `Immediate::~Immediate()`                      | per-step cleanup of Immediate slot          |

## Tail blocks shared by multiple cases

Every case ends by jumping to one of a small set of "common tail" blocks
that finalise the assembled `localList`, splice it into the function-wide
`EvalResults` accumulator at `r14->results`, and run the cascaded
destructor loop on the per-step Immediate / AsmRegister slots. These tail
blocks are at approximately:

- 0x165c90 (used by A.0)
- 0x166a3e (used by BC.0)
- 0x1678e2 (used by A.1 / BC.1)
- 0x16729e (used by A.2 / BC.2)
- 0x1674b2 (used by A.3 / BC.3)
- 0x166e7e (used by BC.4)
- 0x166865 (used by BC.5)
- 0x1676ce (used by A.5)
- 0x165ffa (used by A.4)
- 0x16636b (used by 0x165488 inner step)

Each tail performs the same boilerplate: `lock xadd / __release_weak`
on every shared_ptr held in any of the per-step `vector<Asm>` slots,
then calls `Assembler::~Assembler()` on the entries (loop body at
e.g. 0x164e90, 0x164fc0, 0x165210, 0x1653b0 etc.). These are not
documented per-case because they're emitted by a single shared codegen
template (rg "Assembler::~Assembler" finds 30+ identical 30-byte loops).

## Open questions

1. The exact jt order at `@0x958f50` and `@0x958f68` is inferred from
   linear ordering of case bodies. To verify, dump 24 bytes from the
   binary at those addresses and decode each as `int32_t` offset
   relative to the table base, then add to find the target VA.
2. Whether emit-opcodes 0x10/0x11/0x12 correspond to the existing
   `seqc.user_store_{lo,mid,hi}` opcodes already documented in
   `opcode_map.md` — should cross-reference.
3. `[r12+0x30]` is the variant payload — for varType==2 it's an
    `AsmRegister` (since suser takes one). For other variants the slow
   arm runs `boost::apply_visitor` via vtable @b03dc0; the visitor
   layout was not analysed here.
4. The "addr-as-Immediate" steps (BC.4 step 3, BC.5 step 2 in each
   triplet) effectively *embed* the resolved node address into the
   instruction stream as an immediate. This is the mechanism by which
   user-store opcodes know which device register to target without an
   extra register move.

## Slow-arm error IDs (21b.5)

When `varType` doesn't match the expected arm:

| Case | Expected varType | Error ID | Pattern |
|------|-----------------|----------|---------|
| A.2 | !=2 (scalar) | 0x7f | `format(0x7f, val.toString())` |
| A.3 | !=2 (scalar) | 0x7f | same |
| A.4 | !=2 (scalar) | 0x81 | `format(0x81, val.toString())` |
| A.5 | !=2 (scalar) | 0x82 | `format(0x82, val.toString())` |
| BC.4 | ==2 (register) | 0x7f | `format(0x7f, val.toString())` |
| BC.5 | ==2 (register) | 0x82 | `format(0x82, val.toString())` |

A.0/A.1 slow arms and BC.2/BC.3 slow arms are real codegen paths (not
throws). They use warning 0x80 when `floatEqual(toDouble, toInt)` fails.

## Per-case post-tails (21b.5 discovery, corrected 2026-04-24)

Path-A cases have **per-case post-tail** sequences AFTER the case body
suser(0x17) emission. Each typeIdx has a distinct tail. **Only typeIdx 0
flows through the shared `addi(addr)` common tail at 0x165c90**; all
other cases bypass it.

| typeIdx | Semantic | Post-tail emissions | Closing opcode |
|---------|----------|---------------------|----------------|
| 0 (passthrough) | `addi(addr)` → `suser(0x16)` | 2 steps | 0x16 (commit) |
| 1 (sine pair) | `addi(addr)` → `addi(3)` → `suser(0x10)` → `suser(0x11)` → `toFrequency→addi(freq)` → `suser(0x11)` → `toPhase→addi(phase)` → `suser(0x16)` | 8 steps | 0x16 (commit) |
| 2 (float-bits) | *(none — completes with body's suser(0x17))* | 0 steps | 0x17 |
| 3 (raw-double) | `addi(high32)` → `suser(0x19)` | 2 steps | 0x19 (secondary) |
| 4 (frequency) | `addi(freqHigh32)` → `suser(0x19)` → `addi(addr)` → `suser(0x18)` | 4 steps | 0x18 (freq commit) |
| 5 (phase) | `addi(addr)` → `suser(0x16)` | 2 steps | 0x16 (commit) |

Key post-tail addresses (from disasm trace):
- typeIdx 0: addi(addr) @0x165cdb, suser(0x16) @0x1685d5
- typeIdx 1: addi(addr) @0x166045, addi(3) @0x166148, suser(0x10) @0x1661ac,
  suser(0x11) @0x16867e, toFrequency @0x16875a, addi(freq) @0x1687a0,
  suser(0x11) @0x1688b6, toPhase @0x168981, addi(phase) @0x1689c8,
  suser(0x16) @0x168aea
- typeIdx 3: addi(high32) @0x167610, suser(0x19) @0x168b8e
- typeIdx 4: addi(freqHigh32) @0x167a3e, suser(0x19) @0x168e4e,
  addi(addr) @0x168f47, suser(0x18) @0x169251
- typeIdx 5: addi(addr) @0x167823, suser(0x16) @0x168daa

### suser closing-opcode semantics (refined)

| opcode | meaning | used by |
|--------|---------|---------|
| 0x16 | "commit" — finalise the node write | typeIdx 0, 1, 5 |
| 0x17 | "direct-write" — value register write (also body) | typeIdx 2 (sole) |
| 0x18 | "frequency commit" — frequency-specific finalise | typeIdx 4 |
| 0x19 | "secondary component" — 64-bit upper-32 commit | typeIdx 3 |

### typeIdx 1 (sine pair) extended tail semantics

The sine pair tail configures a full oscillator:
1. I/Q register values set in body (suser 0x17 + 0x19)
2. Node address loaded (addi(addr))
3. Sine node tag 3 written (addi(3) + suser(0x10))
4. Frequency metadata (suser(0x11) + toFrequency → addi(freq))
5. Phase metadata (suser(0x11) + toPhase → addi(phase))
6. Commit (suser(0x16))

### 64-bit split pattern (typeIdx 3, 4)

Both raw-double and frequency use 64-bit values that are split:
- Body emits low32 via addi + suser(0x17)
- Post-tail emits high32 via addi + suser(0x19)
- The `shr rbx, 0x20` instruction at the split point extracts the upper bits
- For frequency, an additional addi(addr) + suser(0x18) follows


