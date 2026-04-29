# Batch 06 — asm_register

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 5 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 1; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 2;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/asm_register.hpp` (header-only)

Site-wide usage surveyed via grep across `reconstructed/src/**.cpp` and
`reconstructed/include/zhinst/**.hpp` (883 hits for `AsmRegister`,
narrowed to direct field access, factory calls, and constructor calls).

Authoritative cross-check: `nm --demangle _seqc_compiler.so`
(93 matches mentioning `AsmRegister`).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AsmRegister` (type) | no | high | Mangled in binary symbols | keep current | not-misnomer |
| `AsmRegister::value` | unsure | low | Generic; role is reg-number | `regNum` (low), keep current (medium) | — |
| `AsmRegister::valid` | no | medium | Used as `valid` flag everywhere | keep current | — |
| `AsmRegister::AsmRegister()` (default) | no | medium | Standard default ctor | keep current | — |
| `AsmRegister::AsmRegister(int v, bool val)` (2-arg) | no | low | Recon convenience; not in binary | keep current | in-scope |
| `AsmRegister::AsmRegister(int)::v` | unsure | low | Generic param name | `regNum` (low), keep current (medium) | — |
| `AsmRegister::AsmRegister(int v, bool val)::val` | yes | medium | Param shadows-ish, opaque | `valid` (medium) | — |
| `AsmRegister::AsmRegister(int)` (1-arg)::n | unsure | low | Generic; sentinel-aware | `regNum` (low), keep current (medium) | — |
| `AsmRegister::Invalid` | no | medium | Recon factory; intent matches | keep current | in-scope |
| `AsmRegister::Reg` | yes | medium | Tautological factory name | `fromIndex` (low), `make` (low), keep current (low) | in-scope |
| `AsmRegister::Reg::n` | unsure | low | Generic param | `regNum` (low), keep current (medium) | — |
| `AsmRegister::magicSkipRegister` | no | high | Mangled in binary symbols | keep current | not-misnomer |
| `AsmRegister::isValid` | no | high | Mangled in binary symbols | keep current | not-misnomer |
| `AsmRegister::toInt` | unsure | low | Recon name; binary uses `operator int` | keep current (medium), drop in favor of `operator int` (low) | in-scope (nm: only operator int() defined) |
| `AsmRegister::operator int` | no | high | Mangled in binary symbols | keep current | not-misnomer |
| `AsmRegister::operator==` | no | high | Mangled in binary symbols | keep current | not-misnomer |
| `AsmRegister::operator==::o` | no | low | Standard "other" param | keep current | — |
| `AsmRegister::operator!=` | no | medium | Standard inverse operator | keep current | — |
| `zhinst::isValid(AsmRegister)` (free) | yes | medium | Not in binary; ambiguous w/ many isValid | keep current (medium), `asmRegIsValid` (low) | in-scope |
| `zhinst::toInt(AsmRegister)` (free) | yes | medium | Not in binary; recon-only wrapper | keep current (medium), `asmRegToInt` (low) | in-scope |
| `zhinst::isValid(AsmRegister)::r` | no | low | Single-letter, idiomatic for tiny inline | keep current | — |
| `zhinst::toInt(AsmRegister)::r` | no | low | Single-letter, idiomatic for tiny inline | keep current | — |

## 3. Detailed findings

### `AsmRegister` (type)  [no / high / not-misnomer]

Evidence:
- nm: `AsmRegister::magicSkipRegister()`, `AsmRegister::AsmRegister(int)`,
  `AsmRegister::isValid() const`, `AsmRegister::operator int() const`,
  `AsmRegister::operator==(AsmRegister) const` — all qualify their methods
  with `AsmRegister::`, no `zhinst::` prefix.
- Header `asm_register.hpp:18` — `struct AsmRegister` defined at global
  scope to match mangling, with a `using ::AsmRegister;` re-export inside
  `zhinst`.
- Used as parameter type in many mangled signatures, e.g.
  `zhinst::AsmCommands::asmPlay(..., AsmRegister, int, AsmRegister, ...)`.

Interpretation:
- The type name `AsmRegister` (at global scope) is encoded in Itanium
  mangling and matches the binary verbatim.

Judgement:
- Tier-1 §3 exclusion. Not a misnomer; cannot be renamed.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/asm_register.hpp:18
- nm: AsmRegister::* (multiple)

---

### `AsmRegister::value`  [unsure / low / —]

Evidence:
- header asm_register.hpp:19 — `int value = -1;  // register number (0-15), or -1 if unset`.
- Direct read sites (only places not going through the methods):
  - assembler.cpp:316,320,324 — `regSrc.value`, `regDst.value`,
    `regAux.value` used to find max register index.
  - seqc_ast_nodes_evaluate.cpp:6704 — `retReg.value` passed to
    `result->setValue(...)` as the register-number argument.
  - resources.cpp:378 — `os << "v: " << var.name << " (Reg: " << var.reg.value << ")\n";`
    serializes the field as the register number.
- Indirect access dominates: `AsmRegister::isValid()` and
  `AsmRegister::operator int()` (which returns `value`) are used
  everywhere else.

Interpretation:
- The field stores a **register index** (or sentinel -1 / 0x7fffffff).
  Every direct read interprets it as such. The name `value` is generic
  but never contradicted; `operator int()` returning `value` is the
  binary-confirmed accessor, so renaming the field would not affect the
  binary-mangled API.

Judgement:
- Not strongly misleading, but generic; specifically `value` collides
  badly with the omnipresent `Value` type and various `.value` fields
  in `Value`, `Address`, `EvalResult` etc. (see grep showing 125 hits
  for `.value`/`.valid` across many unrelated types). A more specific
  name would aid readability without changing semantics.

Proposals:
- `regNum`        (low)
- `index`         (low)
- keep current    (medium)

Locations consulted:
- declared: include/zhinst/asm_register.hpp:19
- read:     src/assembler.cpp:316,320,324; src/seqc_ast_nodes_evaluate.cpp:6704;
            src/resources.cpp:378

---

### `AsmRegister::valid`  [no / medium / —]

Evidence:
- header asm_register.hpp:20 — `bool valid = false;`.
- Direct read sites: assembler.cpp:315,319,323 — `regSrc.valid`,
  `regDst.valid`, `regAux.valid` used to gate consideration of the
  register.
- `AsmRegister::isValid()` (binary-confirmed, asm_register.hpp:35)
  returns `valid`.
- `operator==` semantics (asm_register.hpp:39-46) explicitly use
  `valid` as the validity bit (both invalid → equal regardless of
  value).

Interpretation:
- The field is a true validity flag, used as such everywhere; the
  binary-confirmed accessor `isValid()` reads it directly.

Judgement:
- Name matches usage and aligns with the binary's `isValid()`.

Proposals:
- keep current    (high)

Locations consulted:
- declared: include/zhinst/asm_register.hpp:20
- read:     src/assembler.cpp:315,319,323

---

### `AsmRegister::AsmRegister(int v, bool val)` (2-arg ctor) and its params  [yes / medium / —]

Evidence:
- header asm_register.hpp:24 — `AsmRegister(int v, bool val) : value(v), valid(val) {}`.
- nm shows only `AsmRegister::AsmRegister(int)` (the 1-arg ctor at
  0x28eb40); no 2-arg ctor symbol exists in the binary.
- All uses of the 2-arg form go through brace-init via the static
  factories `Invalid()` / `Reg()` / `magicSkipRegister()`
  (asm_register.hpp:28,29,33). Direct `AsmRegister(v, b)` calls from
  outside are not present in `src/`.

Interpretation:
- The 2-arg ctor is a recon convenience that has no binary counterpart;
  param names were chosen to match the field initializers. `val` is
  ambiguous (could read as "value"), and shadows the conventional
  meaning of "value" while actually meaning "valid".

Judgement:
- The ctor itself is a recon helper (`verify-not-original`), but if
  retained, `val` is a misnomer that risks being read as a value
  rather than a validity flag.

Proposals (for `val`):
- `valid`         (medium)
- `isValid`       (low)
- keep current    (low)

Proposals (for `v`):
- `regNum`        (low)
- `index`         (low)
- keep current    (medium)

Status (for the ctor itself): verify-not-original — not in `nm`.

Locations consulted:
- declared: include/zhinst/asm_register.hpp:24
- nm:       no 2-arg AsmRegister ctor present

---

### `AsmRegister::AsmRegister(int)` (1-arg ctor) and `n`  [no (ctor) / unsure (param) / —]

Evidence:
- nm: `AsmRegister::AsmRegister(int)` at 0x28eb40 — confirmed in binary.
- header asm_register.hpp:25 — `AsmRegister(int n) : value(n > 0 ? n : 0), valid(n >= 0) {}`
  with comment `// @0x28eb40: cmovg + setns`.
- Many call sites: `AsmRegister(0)` (treated as R0, valid=true),
  `AsmRegister(-1)` (treated as invalid, value=0), `AsmRegister(regNum)`
  where `regNum` came from `resources_->getRegisterNumber()`.

Interpretation:
- Ctor itself is binary-attested. The param name `n` is generic; from
  call sites, `n` is consistently a register index (0..15) or -1 for
  invalid. A more descriptive name would help, but `n` is not actively
  misleading.

Judgement:
- Ctor: not a misnomer (binary). Param `n`: generic but harmless.

Proposals (for `n`):
- `regNum`        (low)
- `index`         (low)
- keep current    (medium)

Locations consulted:
- declared: include/zhinst/asm_register.hpp:25
- nm:       `AsmRegister::AsmRegister(int)` @ 0x28eb40
- used:     src/prefetch.cpp:776,2117,2121; src/prefetch_placesingle.cpp:151,157,178,…

---

### `AsmRegister::Invalid`  [no / medium / verify-not-original]

Evidence:
- header asm_register.hpp:28 — `static AsmRegister Invalid() { return {-1, false}; }`.
- Many call sites in resources.cpp:513,656,977,1016,1063,1121,1194,1218,1243,1729,1776,1873.
- nm: no static factory of this name; not present in the binary.

Interpretation:
- Recon-only factory. Returns `{value=-1, valid=false}`, which is the
  documented "no register" sentinel. Name accurately conveys intent.

Judgement:
- Not a misnomer. Status reflects that the symbol does not appear in
  the binary table — synthesis should re-check whether a different
  expression (e.g. inline `AsmRegister(-1)`) is preferred.

Proposals:
- keep current    (high)

Status: verify-not-original

Locations consulted:
- declared: include/zhinst/asm_register.hpp:28
- used:     src/resources.cpp:513,656,977,1016,1063,1121,1194,1218,1243,1729,1776,1873

---

### `AsmRegister::Reg`  [yes / medium / verify-not-original]

Evidence:
- header asm_register.hpp:29 — `static AsmRegister Reg(int n) { return {n, true}; }`.
- nm: no static factory of this name.
- Call sites overwhelmingly pass literal `0`:
  asm_commands.cpp:135,163,357,633,726,732,741,750,756,761,770,782,795,801,846,857,866,1138;
  asm_commands_impl_hirzel.cpp:48,113,134,147; asm_commands_impl_cervino.cpp:45,48,68,77;
  seqc_ast_nodes_evaluate.cpp:8823. Also `AsmRegister::Reg(reg)` in
  resources.cpp:254.

Interpretation:
- `AsmRegister::Reg(...)` reads tautologically — "register register".
  In practice it is used almost exclusively as `AsmRegister::Reg(0)`
  (the R0/zero-register sentinel). The factory does not add semantic
  information beyond the `AsmRegister(int)` ctor.

Judgement:
- The factory name is tautological and the factory itself is
  recon-only. A more descriptive name (or eliminating the factory in
  favor of the binary-attested 1-arg ctor) would be clearer.

Proposals:
- `fromIndex`     (low)
- `make`          (low)
- keep current    (low)
- (alternative: drop and inline `AsmRegister(n)`) — synthesis decision

Status: verify-not-original

Locations consulted:
- declared: include/zhinst/asm_register.hpp:29
- used:     src/asm_commands.cpp:* (many); src/asm_commands_impl_*.cpp:*;
            src/resources.cpp:254; src/seqc_ast_nodes_evaluate.cpp:8823

---

### `AsmRegister::magicSkipRegister`  [no / high / not-misnomer]

Evidence:
- nm: `AsmRegister::magicSkipRegister()` at 0x28ebb0 — verbatim.
- header asm_register.hpp:33 — comment `// 0x28ebb0 — returns
  AsmRegister(INT_MAX, true): sentinel used by splitConstRegisters to
  mark barrier entries that should be stripped.`
- Called in asm_optimize.cpp:1144,1174,1182,1349 as a sentinel marker.

Interpretation:
- Method name is mangled in the binary; cannot be renamed.

Judgement:
- Tier-1 §3 exclusion. Not a misnomer.

Proposals:
- keep current    (high)

Locations consulted:
- declared: include/zhinst/asm_register.hpp:33
- nm:       0x28ebb0

---

### `AsmRegister::isValid`, `operator int`, `operator==`  [no / high / not-misnomer]

Evidence:
- nm:
  - `AsmRegister::isValid() const` @ 0x28eb60
  - `AsmRegister::operator int() const` @ 0x28eb70
  - `AsmRegister::operator==(AsmRegister) const` @ 0x28eb80

Interpretation:
- Method names are mangled in the binary.

Judgement:
- Tier-1 §3 exclusions. Not misnomers.

Proposals:
- keep current    (high)

Locations consulted:
- nm:       0x28eb60, 0x28eb70, 0x28eb80

---

### `AsmRegister::toInt`  [unsure / low / verify-not-original]

Evidence:
- header asm_register.hpp:36 — `int toInt() const { return value; }`.
- nm: no `AsmRegister::toInt() const` symbol; the binary uses
  `operator int()` for the same purpose.
- Free wrapper `zhinst::toInt(AsmRegister r)` (asm_register.hpp:59)
  forwards to it.

Interpretation:
- Recon-only convenience. Duplicates `operator int()` (which is
  binary-attested). Not misleading per se, but redundant.

Judgement:
- Name fits behavior; symbol existence is the question, not the name.

Proposals:
- keep current    (medium)
- (drop in favor of `operator int`) — synthesis decision

Status: verify-not-original

Locations consulted:
- declared: include/zhinst/asm_register.hpp:36

---

### `zhinst::isValid(AsmRegister)` and `zhinst::toInt(AsmRegister)` (free wrappers)  [yes / medium / verify-not-original]

Evidence:
- header asm_register.hpp:58-59 — inline free functions inside
  `namespace zhinst` that simply forward to the member methods.
- nm: no symbols `zhinst::isValid(AsmRegister)` or
  `zhinst::toInt(AsmRegister)`. There **are** unrelated
  `zhinst::isValidUtf8(...)` symbols, increasing the risk of name
  confusion when reading code.
- Header comment (line 56-57) says these exist because "many .cpp
  files call isValid(reg) and toInt(reg) as free functions rather
  than member calls" — i.e., recon adapters, not binary functions.

Interpretation:
- Recon-only adapters. The names `isValid` and `toInt` collide in the
  reader's mind with `zhinst::isValidUtf8` and `zhinst::Value::toInt`
  (also binary-attested). They are not in the binary symbol table at
  all.

Judgement:
- Borderline. The names accurately describe what the wrappers do, but
  in the broader namespace they are too generic. Either keep as-is
  (and tolerate the collision) or rename to something like
  `asmRegIsValid` / `asmRegToInt`. Synthesis should decide whether
  these wrappers are kept at all.

Proposals:
- keep current        (medium)
- `asmRegIsValid` / `asmRegToInt`   (low)
- (drop, force callers to use member)  — synthesis decision

Status: verify-not-original

Locations consulted:
- declared: include/zhinst/asm_register.hpp:58-59
- nm:       no matching free symbols

---

### `AsmRegister::operator==::o`  [no / low / —]

Evidence:
- header asm_register.hpp:39 — `bool operator==(const AsmRegister& o) const`.

Interpretation:
- Conventional one-letter name for "other" in equality operators.

Judgement:
- Not a misnomer.

Proposals:
- keep current    (high)

## 4. Symbols inspected and judged routinely fine

- `AsmRegister::AsmRegister()` (defaulted) — standard, no name to question.
- `AsmRegister::operator!=` — standard inverse operator pattern; name fixed by language.
- `AsmRegister::operator==::o` — conventional "other".
- `zhinst::isValid::r`, `zhinst::toInt::r` — single-letter param fine in
  trivial one-line forwarders.
- `static_assert(sizeof(AsmRegister) == 8, ...)` — not a named symbol
  in the audit sense.

## 5. Coverage

- **Fully covered:** all named symbols declared in
  `include/zhinst/asm_register.hpp` (the type, both ctors, both
  static factories, `magicSkipRegister`, `isValid`, `toInt`, the
  conversion operator, `operator==`/`operator!=`, both fields, and
  the two free-function wrappers in `namespace zhinst`).
- Site-wide usage surveyed via grep across `reconstructed/src/**.cpp`
  and `reconstructed/include/zhinst/**.hpp`. Sample of call sites
  read in `assembler.cpp`, `asm_commands.cpp`,
  `asm_commands_impl_{hirzel,cervino}.cpp`, `prefetch_placesingle.cpp`,
  `prefetch.cpp`, `resources.cpp`, `seqc_ast_nodes_evaluate.cpp`,
  `asm_optimize.cpp`, `custom_functions_play.cpp`.
- Binary cross-checked with `nm --demangle _seqc_compiler.so` (93
  matches mentioning `AsmRegister`).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - `AsmRegister` (the type), `magicSkipRegister`, `isValid`,
    `operator int`, `operator==`, `AsmRegister::AsmRegister(int)` —
    excluded by §3 (mangled in binary). Recorded as `not-misnomer` /
    keep-current blocks per §4d.
  - Cross-batch context: `Node::lengthReg`, `Node::indexOffsetReg`,
    `NodeState::registerHirzel`, `NodeState::registerCervino`,
    `Resources::returnReg_`, `AsmCommands::asmPlay` `reg`/`reg2`
    parameters, and similar `AsmRegister`-typed members/params live
    in their own batches (01, 09, 10, …); not in scope here. The
    canonical `AsmRegister` type name being already excluded means
    those batches can independently propose names for the
    *member/param* identifiers without coordination back to this
    batch.
