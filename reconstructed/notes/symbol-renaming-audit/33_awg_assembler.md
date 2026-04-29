# Batch 33 — awg_assembler

## 1. Files considered

- `reconstructed/include/zhinst/awg_assembler.hpp`
- `reconstructed/src/awg_assembler.cpp`

`AWGAssembler` is a thin pImpl façade — every method forwards a single
argument straight to `AWGAssemblerImpl`. The implementation class
itself is out of scope for this batch (its own audit lives elsewhere).
This batch therefore has very few in-scope symbols: one field, the
parameter names of the forwarding methods, and the reconstructed
forward-declared type `AssemblerInstr`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AssemblerInstr` (fwd-decl, struct tag in `assembleAsmList`) | yes | high | binary type is `zhinst::Assembler` | `Assembler` (high) | — |
| `AWGAssembler::pImpl_` | no | high | standard pImpl pointer | keep current (high) | not-misnomer |
| `AWGAssembler::AWGAssembler::dc` (ctor param) | no | low | matches DeviceConstants role | keep current (medium) | — |
| `AWGAssembler::assembleFile::path` | no | medium | path forwarded as filename | keep current (high) | — |
| `AWGAssembler::assembleString::src` | no | medium | string is asm source text | keep current (high) | — |
| `AWGAssembler::assembleAsmList::asmList` | no | low | list of `Assembler` items | keep current (medium) | — |
| `AWGAssembler::assembleStringToExpressionsVec::src` | no | medium | string is asm source text | keep current (high) | — |
| `AWGAssembler::setMemoryOffset::offset` | no | high | direct offset parameter | keep current (high) | — |
| `AWGAssembler::writeToFile::path` | no | medium | output file path | keep current (high) | — |
| `AWGAssembler::printOpcode::format` | unsure | low | meaning of `int` unverified | keep current (low), `mode` (low) | in-scope (parameter name; nm only preserves int type) |

## 3. Detailed findings

### `AssemblerInstr` (forward declaration used as element type of `assembleAsmList`)  [yes / high / —]

Evidence:
- `awg_assembler.hpp:11`  `struct AssemblerInstr;`
- `awg_assembler.hpp:22`  `void assembleAsmList(std::vector<AssemblerInstr> const& asmList);`
- `nm --demangle _seqc_compiler.so` line 0x2850f0:
  `zhinst::AWGAssembler::assembleAsmList(std::__1::vector<zhinst::Assembler, std::__1::allocator<zhinst::Assembler> > const&)`
- Same symbol table also shows `zhinst::Assembler::~Assembler()`,
  `zhinst::Assembler::Assembler(zhinst::Assembler const&)`,
  `zhinst::Assembler::operator=(zhinst::Assembler&&)` and
  `zhinst::AsmOptimize::isRead(zhinst::Assembler const&, AsmRegister)` —
  i.e. `Assembler` is a *type* (value-typed, copyable, movable).
- Same symbol table shows `zhinst::Assembler::commandToString(zhinst::Assembler::Command)`,
  `zhinst::Assembler::getCmdType(zhinst::Assembler::Command)`, etc. —
  i.e. `Assembler` is *also* used as a class with a nested enum
  `Command` and several static-style helpers.

Interpretation:
- The reconstruction has split a single binary class `zhinst::Assembler`
  (which is both a value type representing one instruction and the home
  of nested helpers like `Assembler::Command`) into two distinct
  reconstructed names: a namespace `Assembler` (used in
  `AsmCommands::alui(Assembler::Command, …)` etc.) and a struct
  `AssemblerInstr` (used here as the element type of `assembleAsmList`).
- Per RULES §3, *type names* present in the binary symbol table are
  authoritative. `zhinst::Assembler` is present; `AssemblerInstr` is
  not. The reconstructed name is therefore the misnomer.
- This is the same observation flagged in batch 26 (assembler) about
  the `Assembler` namespace decomposition being wrong.

Judgement:
- Misnomer: the forward-declared type should be the binary's
  `zhinst::Assembler`.

Proposals:
- `Assembler` (high)

Cross-reference:
- Batch 26 (assembler) records the same root cause from the
  implementation side (namespace `Assembler` is actually a class).
  Batch 10 (`asm_commands`) and batch 25 (`asm_optimize`) both reference
  `Assembler::Command` and `Assembler const&` — they'll also be touched
  by the eventual unification.

Locations consulted:
- declared: include/zhinst/awg_assembler.hpp:11,22
- used:     src/awg_assembler.cpp:35-38
- nm:       0x2850f0 (free)

### `AWGAssembler::pImpl_`  [no / high / not-misnomer]

Evidence:
- `awg_assembler.hpp:34-36`  comment `// unique_ptr semantics (ctor news 0x170, dtor deletes with size 0x170)` and `AWGAssemblerImpl* pImpl_;  // offset 0x00`.
- `awg_assembler.cpp:8`  `pImpl_(new AWGAssemblerImpl(dc))`.
- `awg_assembler.cpp:15-19` destructor explicitly `~AWGAssemblerImpl()` then `operator delete(pImpl_, sizeof(AWGAssemblerImpl))`.
- All eleven forwarding methods access exclusively `pImpl_->…`.

Interpretation:
- Single pointer field, owns the impl, used only for forwarding.
- Name follows the conventional pImpl idiom and matches usage exactly.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

### `AWGAssembler::printOpcode::format`  [unsure / low / verify-not-original]

Evidence:
- `awg_assembler.hpp:31`  `void printOpcode(int format) const;`
- `awg_assembler.cpp:74-77` forwards directly to `pImpl_->printOpcode(format)`.
- No use sites of the wrapper visible in this batch; the impl is in a
  separate batch and was not opened.

Interpretation:
- The name `format` suggests a print format selector (e.g. hex/bin/asm),
  but the wrapper does nothing with the value beyond forwarding. Without
  reading the impl it cannot be confirmed whether the parameter selects
  a textual format, an output destination, a verbosity level, or
  something else.

Judgement:
- Unsure — the name is plausible but unverified at the wrapper level;
  the impl's body should decide it.

Proposals:
- keep current (low)
- `mode` (low)

Cross-reference:
- Resolution belongs in the `AWGAssemblerImpl::printOpcode` audit (impl
  batch, not yet identified by number here).

## 4. Symbols inspected and judged routinely fine

- `AWGAssembler::AWGAssembler::dc` — single ctor parameter forwarded to `AWGAssemblerImpl(dc)`; matches `DeviceConstants const&` type and the established `dc` convention used elsewhere in the project.
- `AWGAssembler::assembleFile::path` — passed straight to a method named `assembleFile`; "path" is the obvious role.
- `AWGAssembler::assembleString::src` / `assembleStringToExpressionsVec::src` — the string fed to a SeqC-asm parser is uniformly called `src` across this codebase.
- `AWGAssembler::assembleAsmList::asmList` — vector of `Assembler` instructions; name describes what it is. (The element-*type* misnomer is filed separately above.)
- `AWGAssembler::setMemoryOffset::offset` — single `unsigned int` argument to a setter named `setMemoryOffset`; tautological match.
- `AWGAssembler::writeToFile::path` — output file path; matches.
- `AWGAssembler::getOpcode` / `getReport` / `printOpcode` — method names are present in the symbol table (0x285140 / 0x285150 / 0x285170) and so are out of scope per RULES §3.

## 5. Coverage

**Fully covered:**
- All in-scope symbols declared in `awg_assembler.hpp` and defined in
  `awg_assembler.cpp` were inspected.
- The forward-declared element type `AssemblerInstr` was verified
  against the binary symbol table.

**Deferred:**
- Semantic verification of `printOpcode::format` requires reading the
  impl method body, which lives in a separate batch
  (`awg_assembler_impl`). Recorded with status `verify-not-original`.

**Not covered (out of scope per RULES §2/§3):**
- All public method names of `AWGAssembler`
  (`assembleFile`, `assembleString`, `assembleAsmList`,
  `assembleStringToExpressionsVec`, `setMemoryOffset`, `writeToFile`,
  `getOpcode`, `getReport`, `printOpcode`) — all confirmed present in
  the binary symbol table at addresses 0x2850d0–0x285170 and so
  authoritative under §3.
- Class name `AWGAssembler` — present in the symbol table; authoritative.
- Forward-declared types `AWGAssemblerImpl` and `DeviceConstants` are
  type names whose audits belong to their own batches.
- Type `AsmExpression` (used in `assembleStringToExpressionsVec` return)
  is present in the symbol table (`zhinst::AsmExpression::~AsmExpression`,
  etc.); authoritative under §3.
