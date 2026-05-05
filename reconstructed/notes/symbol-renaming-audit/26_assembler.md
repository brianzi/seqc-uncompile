# Batch 26 — assembler

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 2 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 1.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/asm/assembler.hpp`
- `reconstructed/src/asm/assembler.cpp`

Cross-file usage surveyed in: `asm_list.cpp`, `asm_list.hpp`,
`asm_optimize.cpp`, `awg_assembler_opcodes.cpp`,
`asm_commands_impl_hirzel.cpp`, `asm_commands_impl_cervino.cpp`,
`asm_commands_impl.hpp`, `asm_commands.hpp`, `asm_commands.cpp`,
`asm_parser_context.cpp`, `awg_assembler.cpp`,
`awg_assembler_impl_pipeline.cpp`, `awg_compiler.cpp`,
`prefetch.hpp`, `prefetch_emit.cpp`, `prefetch_placesingle.cpp`,
`magic_numbers_proposal.md` (constants), and the symbol-table
audit notes for batches 10, 49, 50.

Symbol-table verification (`nm --demangle _seqc_compiler.so`):

- The reconstruction has split the binary's `Assembler` into
  `namespace Assembler` + `struct AssemblerInstr`. `nm` shows:
  - `zhinst::Assembler::Assembler(zhinst::Assembler const&)` (copy ctor)
  - `zhinst::Assembler::~Assembler()`
  - `zhinst::Assembler::operator=(zhinst::Assembler&&)`
  - `zhinst::Assembler::operator=(zhinst::Assembler const&)`
  - `zhinst::Assembler::highestRegisterNumber() const`
  - `zhinst::Assembler::str(bool) const`
  - `zhinst::Assembler::Command` appearing as a *nested* type inside
    `Assembler` (e.g. `zhinst::AsmCommands::alui(zhinst::Assembler::Command, ...)`)
  - `zhinst::Assembler::PlayDummyType` likewise nested.
  - Free functions `zhinst::Assembler::commandToString`,
    `commandFromString`, `getCmdType`, `getOpcodeType`, `getCycles`,
    `getRegisterOrder` — these are static methods of class
    `Assembler`, not free functions in a namespace.

  → The type name `AssemblerInstr` and the `namespace Assembler`
  decomposition are reconstruction artefacts. The binary's class name
  is `Assembler`. Because `commandToString(Command)` etc. are members
  of class `Assembler`, all the "namespace-scope" free functions in
  the header are tier-1 method-name **excluded** from rename of those
  function-symbols themselves; the type-name re-derivation is in
  scope (§3, "Class/struct/enum type names — authoritative").

- Free function `zhinst::str(zhinst::AsmOperationType)` appears in
  `nm` → free-function name **excluded** (§3, tier 1). The type name
  `AsmOperationType` itself is in scope.

- `Assembler::Command`, `Assembler::PlayDummyType` appear nested in
  parameter lists (e.g. `AsmCommands::wvfs(Assembler::PlayDummyType, …)`)
  → those *enum-type* names are tier-1 **excluded**.

- Class has **no static data members** appearing in `nm`. Nothing to
  exclude on that basis.

- Enum members (`END`, `NOP`, `ADDI`, …, `Type0`, `Type1`,
  `CmdType_None…`, `RegOrder::None…`, `OpcodeFormat::NoArg…`),
  data members (`cmd`, `immediates`, `regSrc`, `regDst`, `regAux`,
  `outputs`, `label`, `comment`), parameter names (`cmd`, `name`,
  `verbose`, `t`, `instr`), the inline helper `isWaveformCmd`, the
  `kCycle_*` constants, and the `PlayDummyType` member names — all
  **in scope** (§2, §3).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AssemblerInstr` (type) | yes | high | binary class is `Assembler` | Assembler (high) | not-misnomer counterpart for ns | coordinated-rename |
| `namespace Assembler` (type-decomposition) | yes | high | reconstruction artefact | nest enums/statics into class Assembler (high); keep current (low) | coordinated-rename |
| `Assembler::Command` (enum type) | no | high | tier-1 binary symbol | keep current (high) | not-misnomer |
| `Assembler::PlayDummyType` (enum type) | no | high | tier-1 binary symbol | keep current (high) | not-misnomer |
| `Assembler::Command::END..FB` (members) | no | medium | match cmdMap strings | keep current (high) | — |
| `PlayDummyType::Type0`, `Type1` (members) | yes | medium | placeholder names | concrete role names; keep current (low) | — |
| `kCycle_Unknown`, `kCycle_Single`, `kCycle_BranchPenalty` | no | medium | role-descriptive | keep current (high) | — |
| `CmdType_None..RegReg` (enumerators) | no | medium | role-descriptive | keep current (high) | — |
| `RegOrder::None..DestImmSrc` (enumerators) | unsure | low | derived from disassembly | keep current (medium); see §3 | — |
| `OpcodeFormat::NoArg..DualImm14` (enumerators) | no | low | encoding-format names | keep current (high) | — |
| `AsmOperationType` (type) | no | high | not in nm; faithful str() keys | keep current (high) | not-misnomer |
| `AsmOperationType::Cmd/Name/Value/Reg` | no | high | match str() literals | keep current (high) | not-misnomer |
| `AssemblerInstr::cmd` (field) | no | high | matches usage everywhere | keep current (high) | — |
| `AssemblerInstr::immediates` (field) | no | medium | input-operand vector | keep current (high) | — |
| `AssemblerInstr::regSrc` (field) | no | high | renamed Phase 27a | keep current (high) | — |
| `AssemblerInstr::regDst` (field) | no | high | renamed Phase 27a | keep current (high) | — |
| `AssemblerInstr::regAux` (field) | no | high | renamed Phase 27a | keep current (high) | — |
| `AssemblerInstr::outputs` (field) | unsure | low | comment notes simplify-zero use | keep current (medium); outputsOrZeroCheck (low) | — |
| `AssemblerInstr::label` (field) | no | high | branch-target string | keep current (high) | — |
| `AssemblerInstr::comment` (field) | no | high | trailing `// comment` | keep current (high) | — |
| `commandToString::cmd` (param) | no | high | only use is the lookup | keep current (high) | — |
| `commandFromString::name` (param) | no | high | string → command lookup | keep current (high) | — |
| `commandFromString::lower` (local) | no | high | the lowercased copy | keep current (high) | — |
| `getOpcodeType::cmd` (param) | no | high | switch operand | keep current (high) | — |
| `getCycles::cmd` (param) | no | high | switch operand | keep current (high) | — |
| `getCmdType::cmd` (param) | no | high | switch operand | keep current (high) | — |
| `getRegisterOrder::cmd` (param) | no | high | switch operand | keep current (high) | — |
| `getCmdMap` (free fn) | no | medium | reconstruction helper | keep current (high) | — |
| `AssemblerInstr::str::verbose` (param) | no | high | controls comment append | keep current (high) | — |
| `AssemblerInstr::str::needComma` (local) | yes | medium | dead-write logic; misleading | (local; out of scope tweak) keep current (medium) | — |
| `AssemblerInstr::str::result` (local) | no | high | the built string | keep current (high) | — |
| `AssemblerInstr::str::ss` (local) | no | high | conventional ostringstream | keep current (high) | — |
| `AssemblerInstr::str::imm` (local) | no | high | range-for of `immediates`/`outputs` | keep current (high) | — |
| `AssemblerInstr::str::r` (lambda param) | no | high | the AsmRegister | keep current (high) | — |
| `AssemblerInstr::highestRegisterNumber::maxReg` (local) | no | high | running max | keep current (high) | — |
| `AssemblerInstr::highestRegisterNumber::found` (local) | no | high | flag for "any valid" | keep current (high) | — |
| `AssemblerInstr::operator=::other` (param) | no | high | conventional | keep current (high) | — |
| `str(AsmOperationType)::t` (param) | no | medium | very short, switch operand | keep current (high) | — |
| `isWaveformCmd::instr` (param) | yes | high | misnomer pair: name/role mismatch | keep current; rename function (high) | cross-batch-arbitration |
| `isWaveformCmd` (free fn name) | yes | high | predicate name lies about meaning | isControlOpcode, isMessageOpcode | cross-batch-arbitration |

Group order: types → enums → enum members → constants → fields → free
functions (with their params/locals) → methods (with their
params/locals) → standalone helpers.

## 3. Detailed findings

### `AssemblerInstr` (type) + `namespace Assembler`  [yes / high / coordinated-rename]

Evidence:
- `nm --demangle _seqc_compiler.so | grep Assembler::` shows method
  symbols `zhinst::Assembler::~Assembler()`,
  `zhinst::Assembler::Assembler(zhinst::Assembler const&)`,
  `zhinst::Assembler::operator=(zhinst::Assembler&&)`,
  `zhinst::Assembler::operator=(zhinst::Assembler const&)`,
  `zhinst::Assembler::highestRegisterNumber() const`,
  `zhinst::Assembler::str(bool) const`. C++ does not allow
  destructors/constructors/assignment operators on a namespace —
  `Assembler` must be a class.
- The same `nm` output qualifies static helpers as
  `zhinst::Assembler::commandToString(zhinst::Assembler::Command)`,
  `zhinst::Assembler::getCmdType(zhinst::Assembler::Command)`, etc.
  Static methods of class `Assembler` mangle identically to free
  functions in `namespace Assembler`, so this alone does not decide
  it — but combined with the ctor/dtor it does.
- `Assembler::Command` and `Assembler::PlayDummyType` appear as
  *nested* names in many other mangled symbols
  (`AsmCommands::alui(zhinst::Assembler::Command, …)`,
  `AsmCommandsImpl::wvfs(zhinst::Assembler::PlayDummyType, …)`),
  consistent with class-nested enums.
- Reconstruction header: `assembler.hpp:14-139` declares
  `namespace Assembler { enum Command…; std::string commandToString(…);
  … }` and `assembler.hpp:172` declares `struct AssemblerInstr` at
  namespace scope.
- Site-wide every consumer types `AssemblerInstr` *and* qualifies
  enums with `Assembler::Command`, `Assembler::PlayDummyType` (52
  matches across `asm_list.cpp`, `asm_optimize.cpp`,
  `awg_assembler_opcodes.cpp`, `asm_parser_context.cpp`, etc.).

Interpretation:
- The binary has a single class `Assembler` containing nested
  `Command`/`PlayDummyType` enums, the static dispatch helpers, and
  the per-instruction state plus instance methods. The reconstruction
  split this into a namespace + a struct named `AssemblerInstr`. The
  type name `AssemblerInstr` does not appear anywhere in the binary
  symbol table.

Judgement:
- Yes — `AssemblerInstr` is a misnomer (binary type is `Assembler`)
  and the `namespace Assembler` is a structural artefact, not a
  binary entity.

Proposals:
- Rename `AssemblerInstr` → `Assembler`, fold the namespace's
  contents into it (enums become nested, free helpers become static
  methods)  (high)
- Keep current decomposition for ergonomics (the namespace lets
  consumers write `Assembler::Command` without disturbing the
  per-instruction class name)  (low)

Cross-reference:
- This rename has wide ripple effects (every file currently typing
  `AssemblerInstr` and every `Assembler::Command` qualification).
  Synthesis should plan it together with the eventual fold.

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:16,139,172`
- used: `src/asm/asm_list.cpp` (≥20 sites), `src/asm/asm_optimize.cpp` (≥10),
  `src/codegen/awg_assembler_opcodes.cpp:38,194,219,263,330,539,598,606,628`,
  `src/asm/asm_parser_context.cpp:292,357,377`,
  `src/asm_commands_impl_*.cpp` (all `result.assembler` / `result.isWaveformCmd`)

---

### `Assembler::Command` (enum type)  [no / high / not-misnomer]

Evidence:
- Tier-1: `nm` mangles parameter types as
  `zhinst::Assembler::Command` (e.g.
  `zhinst::AsmCommands::alui(zhinst::Assembler::Command, …)`,
  `zhinst::Assembler::commandToString(zhinst::Assembler::Command)`).
- The map keyed by these values is named `cmdMap` and produces the
  human-visible mnemonics (`"end"`, `"nop"`, `"addi"`, …).
- Alternative term `opcode` is used in the header but only inside
  comments describing classification helpers; the *value* name in
  binary is `Command`.

Judgement:
- No — name matches binary tier-1 symbol.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:20`
- used: `src/asm/asm_list.cpp:378,407,476,488`, `src/asm/asm_parser_context.cpp:357,377`,
  `src/codegen/awg_assembler_opcodes.cpp:194,219,…`

---

### `Assembler::PlayDummyType` + `::Type0`, `::Type1`  [type=no / members=yes / medium / —]

Evidence:
- Tier-1: `nm` shows the type qualified as
  `zhinst::Assembler::PlayDummyType` in `wvfs` signatures
  (`AsmCommandsImpl{,Hirzel,Cervino}::wvfs`, `Prefetch::wvfs`,
  `AsmCommands::wvfs`).
- Header `assembler.hpp:81-84` declares only two members named
  `Type0 = 0`, `Type1 = 1`.
- Use site: `src/codegen/prefetch_placesingle.cpp:517-518` — the value is
  produced by `static_cast<Assembler::PlayDummyType>(npD->config.hold)`
  i.e. directly from a `hold` field.
- No `str()` for this enum, no JSON serializer, no log string giving
  authoritative names for the members.

Interpretation:
- The type name itself is in the binary symbol table; the member
  names are reconstruction guesses (`Type0`/`Type1`) that encode no
  information beyond their underlying integer values.

Judgement:
- Type: no — keep `PlayDummyType`. Members: yes (low confidence)
  — they are placeholder names. The roles (likely `Hold` vs
  something else, given `npD->config.hold`) are not yet pinned down
  with strong evidence here.

Proposals:
- Members: investigate at synthesis time, e.g. `Off` / `Hold`
  (low), or `Type0` / `Type1` kept until evidence settles
  (medium).

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:81-84`
- used: `src/codegen/prefetch_placesingle.cpp:517-518`,
  `src/asm/asm_commands_impl_hirzel.cpp:75`,
  `src/asm/asm_commands_impl_cervino.cpp:86`, `src/asm/asm_commands.cpp:129`,
  `src/codegen/prefetch_emit.cpp:762`

---

### `AssemblerInstr::regSrc / regDst / regAux` (fields)  [no / high / —]

Evidence:
- Header comment `assembler.hpp:154-171` documents the Phase-15c +
  Phase-27a corrected layout:
  - `regSrc` (+0x20): READ when `cmdType & 1`
  - `regDst` (+0x28): WRITTEN when `cmdType & 2`
  - `regAux` (+0x30): READ if `cmdType ∈ {1,7}`, WRITTEN if `cmdType == 7`
- `src/asm/asm_optimize.cpp:154-204,547-611` has matching offset/role
  comments at every read/write site.
- `src/asm/assembler.cpp:311-330` (`highestRegisterNumber`) and
  `src/asm/assembler.cpp:355-364` (the `str()` register printer) treat
  the three fields uniformly, just selecting which one is the
  destination/source as appropriate.

Interpretation:
- The names match the documented register roles uniformly across
  every reader.

Judgement:
- No — this is a deliberately renamed set whose new names match
  use sites well.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:176-178`
- used: `src/asm/assembler.cpp:311-330,355-364,449-451,465-467`,
  `src/asm/asm_optimize.cpp:73-94,154-204,547-611`

---

### `AssemblerInstr::immediates` and `AssemblerInstr::outputs` (fields)  [immediates=no / outputs=unsure / low / —]

Evidence:
- `src/asm/assembler.cpp:349-352,367-370` — `str()` writes `immediates`
  before the registers and `outputs` after them, in disassembly form.
- `assembler.hpp:179` comment: "OUTPUT operands (also used for ADDI
  zero-check by simplifyAssign)".
- Site survey: callers populate `immediates` from operand-construction
  helpers; `outputs` is populated more sparsely and is also probed by
  `simplifyAssign` (per the header comment).

Interpretation:
- `immediates` is the input-operand list, used uniformly.
  `outputs` carries instructional outputs *and* serves a secondary
  zero-check role for ADDI optimization. The dual purpose is a known
  oddity already documented but does not contradict the name in the
  primary use.

Judgement:
- `immediates`: no, name fits primary role; `outputs`: unsure — the
  ADDI-zero-check usage is not what "outputs" suggests, but it is a
  minority site.

Proposals:
- `immediates`: keep current  (high)
- `outputs`: keep current  (medium); rename only if synthesis
  decides to surface the secondary use, e.g. `outputsOrZeroCheck`
  (low)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:175,179`
- used: `src/asm/assembler.cpp:349-353,367-370,448,452,464,468`

---

### `kCycle_Unknown`, `kCycle_Single`, `kCycle_BranchPenalty`  [no / medium / —]

Evidence:
- `assembler.hpp:99-103` declares them as documentation for
  `getCycles`'s return value.
- They are not in `nm` (file-local constexpr; no external linkage).
- `src/asm/assembler.cpp:160-205` returns the bare integers `1`, `3`, `0`
  rather than the named constants. No call site uses the names.

Interpretation:
- The constants are documentation-only, never referenced. They are
  not misnamed — they describe the values they represent — and they
  are not currently misleading anyone, since nothing reads them.

Judgement:
- No.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:99-103`
- used: nowhere in the codebase (verified by grep)

---

### `CmdType_None`, `CmdType_Read`, `CmdType_Write`, `CmdType_ReadWrite`, `CmdType_RegReg`  [no / medium / —]

Evidence:
- `assembler.hpp:107-113` declares the enum and documents the
  register-access semantics: bit 0 = reads, bit 1 = writes, with
  the special `RegReg=7` combining read+write+aux-write.
- `src/asm/assembler.cpp:216-254` matches this: returns `1` for
  read-only opcodes (PRF/WVF/BRZ/ST/...), `2` for LD (write only),
  `3` for ALU-immediate (read + write), `7` for ALU-reg-reg
  (read + write + aux-write).
- The semantics are also encoded in `src/asm/asm_optimize.cpp:73-94`
  (`isRead`, `isWritten`) by bit-anding against 1 and 2 respectively.

Interpretation:
- Names accurately describe the bitmask semantics.

Judgement:
- No.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:107-113`
- used: `src/asm/asm_optimize.cpp:73-94,908`

---

### `RegOrder::None`, `SourceOnly`, `DestOnly`, `ThreeReg`, `DestImmSrc`  [unsure / low / —]

Evidence:
- `assembler.hpp:117-123` declares the enum with comments:
  - `None = 0` — no registers
  - `SourceOnly = 1` — one reg → reg2
  - `DestOnly = 2` — one reg → reg0
  - `ThreeReg = 3` — two regs → reg1, reg2
  - `DestImmSrc = 4` — two regs → reg0, reg2
- `src/asm/assembler.cpp:263-301` returns these integer values; `3` is
  used for ALU-reg-reg *and* for PRF/WVF/WVFI/WTRIG (which have one
  source register plus immediates) — i.e. the `ThreeReg` label
  isn't quite literal at every site (PRF/WVF use two regs *and*
  immediates).
- The values 0..4 map cleanly onto the four register-shape cases,
  but the label `ThreeReg=3` mixes "true three-register reg-reg" with
  "two-register-plus-immediate WVF". The header itself flags this
  with the dual comment "two regs → reg1, reg2".

Interpretation:
- The enum is a categorisation that fuses two superficially similar
  layouts; the current names emphasise the shared shape rather than
  the underlying instruction class. The labels are not wrong but are
  potentially misleading on first reading.

Judgement:
- Unsure — names are defensible for the categorisation chosen, but
  `ThreeReg` over-promises register count.

Proposals:
- keep current  (medium)
- `TwoSrc` / `TwoRegImm` for value 3; keep others  (low)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:117-123`
- used: only the dispatcher/table in disassembly; no symbolic
  references in source.

---

### `AsmOperationType` (type) + members  [no / high / not-misnomer]

Evidence:
- `nm` shows free function `zhinst::str(zhinst::AsmOperationType)`
  → tier-1 type-name **excluded**.
- Tier-2 (faithful string evidence): `src/asm/assembler.cpp:476-484`
  maps members to the literal strings `"cmd"`, `"name"`, `"value"`,
  `"reg"` — the same strings noted in the header comment as coming
  from binary `.rodata` at `0x28d280`.
- Member identifiers (`Cmd`, `Name`, `Value`, `Reg`) match the
  string literals 1:1 in capitalised form.

Interpretation:
- The type and its members are anchored by both the binary symbol
  table and `.rodata` strings.

Judgement:
- No — positive evidence the names are correct.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:143-149`
- used: `src/asm/assembler.cpp:476-484`

---

### `commandFromString::lower` (local), `getCmdMap` (free fn)  [no / high / —]

Evidence:
- `src/asm/assembler.cpp:91-98`: `std::string lower =
  boost::to_lower_copy(name); … map.find(lower)`. The local holds
  exactly the lowercased input.
- `src/asm/assembler.cpp:26-73`: `getCmdMap()` returns the function-local
  `static const std::map<…> cmdMap`.

Interpretation:
- Names match observed roles directly.

Judgement:
- No.

Proposals:
- keep current  (high)

Locations consulted:
- `src/asm/assembler.cpp:26-98`

---

### `AssemblerInstr::str::needComma` (local)  [yes / medium / —]

Evidence:
- `src/asm/assembler.cpp:346-369`:
  ```
  bool needComma = false;
  for (const auto& imm : immediates) {
      ss << " " << toString(imm);
      needComma = true;
      if (needComma) ss << ",";   // always true on this path
  }
  …
  emitReg lambda sets needComma = true (read nowhere)
  outputs loop sets needComma = true (read nowhere)
  label branch sets needComma = false (read nowhere)
  ```
- The variable is read on exactly one line (`if (needComma) ss << ","`),
  immediately after being unconditionally set to `true`. All other
  writes are dead. The trailing-comma stripping at lines 379-382 is
  what actually prevents an extra comma at end-of-line.

Interpretation:
- The local was clearly intended to control comma emission, but in
  the current code it does no useful work. The name describes an
  intent that the implementation no longer carries out.

Judgement:
- Yes (mildly) — the name overstates the variable's role. Locals
  are only flagged when "obviously misleading" (§2). This is a low-
  stakes one but visible to readers.

Proposals:
- keep current  (medium) — local; cleanup is more a code change
  than a naming one.
- (synthesis may instead drop the variable entirely as part of
  refactoring `str()`)

Locations consulted:
- `src/asm/assembler.cpp:346-376`

---

### `isWaveformCmd` (free function) + `isWaveformCmd::instr` (param)  [yes / high / cross-batch-arbitration]

Evidence:
- Header `assembler.hpp:202-205`:
  ```
  // isWaveformCmd: (cmd - 3) < 3u, i.e. cmd in {MESSAGE=3, 4, ERROR_MSG=5}
  inline bool isWaveformCmd(const AssemblerInstr& instr) {
      return (static_cast<uint32_t>(instr.cmd) - 3u) < 3u;
  }
  ```
  By its own comment, the predicate is true for MESSAGE, opcode 4,
  and ERROR_MSG — which are explicitly the **non-waveform**,
  control / message pseudo-opcodes (the waveform opcodes are
  PRF/WVF/WVFI/WVFS_H at much higher values).
- The `Asm::isWaveformCmd` boolean field (asm_list.hpp:61) is set
  by `isWaveformCmd(instr)` in many call sites
  (`asm_commands_impl_cervino.cpp:60,80,122,135,148`,
  `asm_commands_impl_hirzel.cpp:61,88,102,136,149,162`).
- `src/asm/asm_list.cpp:200`: the field is checked by
  `if (entry.isWaveformCmd && opcode != 3 && opcode != 4 && opcode != 5)`
  i.e. the field is true for {MESSAGE, 4, ERROR_MSG} and the
  conditional then suppresses exactly those opcodes — the field is
  used as a guard for *control/message* opcodes, not waveform ones.
- `asm_commands_impl_cervino.cpp:109` and
  `asm_commands_impl_hirzel.cpp:123`: `result.isWaveformCmd = flag;`
  where `flag` is the misnomer flagged in batch 49
  (`AsmCommandsImpl::brz::flag`). Batch 49 proposed renaming `flag`
  → `isWaveformCmd` to match the field; that arbitration assumed the
  *field* name was correct.
- Also: the `isWaveformCmdOverride_` bool on `AsmExpression`
  (`asm_expression.hpp:120`) feeds into the same field and inherits
  the same naming.

Interpretation:
- The predicate, the field, and the override all share the name
  `isWaveformCmd`, but the value is true for MESSAGE / opcode-4 /
  ERROR_MSG opcodes — precisely the control-flow / message pseudo-
  instructions, *not* the waveform instructions. The name is
  inverted semantically from what the data flowing through it
  represents.
- Either:
  (a) the predicate genuinely encodes "is this a control/message
  opcode" and all three names should change together; or
  (b) the field is intended to mean "should this entry be
  subjected to waveform-style optimisation suppression" with the
  truth condition being the opposite of "is a waveform opcode" — in
  which case the name is still wrong but in a subtler way.

Judgement:
- Yes — the predicate and the param it takes are misnomers; the
  truth condition does not include any waveform opcode.

Proposals:
- Free fn: rename to `isControlOpcode` or `isMessageOpcode`  (high)
- Param: keep `instr` (high) — only the function name is wrong.
- keep current  (low)

Cross-reference:
- Counterpart `Asm::isWaveformCmd` field lives in batch covering
  `asm_list.hpp` (not yet audited; tracked via this entry).
- Batch 49 (`AsmCommandsImpl::brz::flag` → `isWaveformCmd` proposal)
  must be re-evaluated — if `isWaveformCmd` is itself the wrong
  name, the rename target should track whatever name the field
  receives.
- Batch 10 also deferred to the field name; same dependency.

Locations consulted:
- declared: `include/zhinst/asm/assembler.hpp:202-205`
- used: `src/asm/asm_list.cpp:357,399,418,565,572,200`,
  `src/asm/asm_commands_impl_cervino.cpp:33,60,80,109,122,135,148`,
  `src/asm/asm_commands_impl_hirzel.cpp:26,37,61,88,102,123,136,149,162`,
  `src/asm/asm_optimize.cpp:343,454`
- field decl: `include/zhinst/asm/asm_list.hpp:61`
- override decl: `include/zhinst/asm/asm_expression.hpp:120`

---

## 4. Symbols inspected and judged routinely fine

- `Assembler::Command` enumerators (`END`, `NOP`, `LABEL`, `MESSAGE`,
  `ERROR_MSG`, `PRF`, `WVF`, `WVFI`, `WVFS_H`, `ADDI`, `ADDIU`,
  `ADDR`…`XORR`, `ANDI`…`XNORIU`, `LD`, `WTRIG`, `WPRF`, `WWVF`,
  `CWVF`, `BRZ`, `BRNZ`, `BRGZ`, `ST`, `TRAP`, `IRPT`, `CWVFR`,
  `WVFE`, `WVFEI`, `WVFET`, `WTRIGI`, `JMP`, `FB`, `INVALID`) —
  match the lowercase keys in `getCmdMap()` 1:1 (faithful tier-2
  string evidence, `src/asm/assembler.cpp:28-71`).
- `OpcodeFormat::NoArg`, `RegImm20`, `RegTripleImm8`, `DualRegImm20`,
  `Complex`, `DualImm14` — comments at declaration describe what the
  encoding does and the names are self-consistent. Not currently
  used by any in-tree source (lookup table at 0x95d094 in binary).
- `AssemblerInstr::label`, `comment` — used exactly as their names
  suggest in `str()` (lines 339, 374, 389) and in copy/move assign.
- `commandToString::cmd`, `getOpcodeType::cmd`, `getCycles::cmd`,
  `getCmdType::cmd`, `getRegisterOrder::cmd`, `commandFromString::name`
  — single-use switch/lookup operands; names match.
- `AssemblerInstr::operator=::other` — conventional.
- `AssemblerInstr::str::ss`, `result`, `imm`, `r` (lambda) — short
  scope, conventional names matching their contents.
- `AssemblerInstr::highestRegisterNumber::maxReg`, `found` — running
  max and presence flag, both used as named.
- `str(AsmOperationType)::t` — single-letter parameter; idiomatic for
  a one-arg conversion.
- `AssemblerInstr::cmd` — uniform shorthand for the opcode; widely
  used by that name across the codebase.

## 5. Coverage

- **Fully covered:**
  - `assembler.hpp`: every named entity (type, namespace, enum, enum
    member, named constant, struct field, free-function declaration,
    inline helper, parameter).
  - `assembler.cpp`: every function definition, every named local,
    plus the `getCmdMap` helper and the global `cmdMap` literal.
- **Deferred:**
  - The full `AssemblerInstr` → `Assembler` rename and consequent
    namespace fold is a coordinated cross-batch change; recorded as
    `coordinated-rename` here, awaiting synthesis.
  - The `isWaveformCmd` field on `AsmList::Asm` (declared in
    `asm_list.hpp`) and the `isWaveformCmdOverride_` field on
    `AsmExpression` are out-of-batch; flagged via cross-reference.
- **Not covered (out of scope per RULES §2/§3):**
  - Method names `commandToString`, `commandFromString`,
    `getOpcodeType`, `getCycles`, `getCmdType`, `getRegisterOrder`,
    `highestRegisterNumber`, `str`, `~AssemblerInstr`,
    `operator=` — all appear in `nm` qualified by `Assembler::`,
    tier-1 excluded.
  - The free function name `str(AsmOperationType)` — appears in
    `nm` as `zhinst::str(zhinst::AsmOperationType)`, tier-1 excluded.
  - The enum type names `Assembler::Command`,
    `Assembler::PlayDummyType`, `AsmOperationType` — tier-1
    excluded; recorded as positive-evidence blocks for the type
    name only.
