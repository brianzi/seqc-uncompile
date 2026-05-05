# Batch 49 — asm_commands_impl

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 2 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 2.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/asm/asm_commands_impl.hpp`
- `reconstructed/src/asm/asm_commands_impl.cpp`
- `reconstructed/src/asm/asm_commands_impl_hirzel.cpp`
- `reconstructed/src/asm/asm_commands_impl_cervino.cpp`

Cross-file usage surveyed in: `asm_commands.cpp` (the only direct
caller of `impl_->...` virtuals; the wrapper is `AsmCommands::*`).

Symbol-table verification (`nm --demangle _seqc_compiler.so`):

- Types `zhinst::AsmCommandsImpl`, `zhinst::AsmCommandsImplHirzel`,
  `zhinst::AsmCommandsImplCervino` — appear as typeinfo/vtable
  symbols → **excluded** (§3, tier 1).
- All thirteen virtual methods (`isCWVFRSupported`, `wwvfq`, `wprf`,
  `wvf`, `wvfi`, `wvfs`, `wvft`, `brz`, `ssl`, `ssr`, `ldiotrig`,
  destructors) appear in `nm` qualified by both subclass names →
  **method names excluded** (§3, tier 1).
- Free function `AsmCommandsImpl::getInstance(AwgDeviceType)` appears
  in `nm` → method name **excluded**.
- Class has **no data members** (abstract base + two empty derived).
  No static data members appear in `nm`. Nothing to inspect there.

In-scope symbols: parameter names of the 13 virtuals (×3 — base
declarations + Cervino + Hirzel overrides), the static factory's
parameter, and a handful of locals in the factory.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AsmCommandsImpl::getInstance::deviceType` | no | high | matches enum type, only use | keep current (high) | — |
| `AsmCommandsImpl::getInstance::val` (local) | no | low | conventional int cast | keep current (high) | — |
| `AsmCommandsImpl::getInstance::shifted` (local) | no | low | role-descriptive | keep current (high) | — |
| `AsmCommandsImpl::getInstance::mask` (local) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommandsImpl::wwvfq::lineNumber` (param) | no | high | passed straight to wavetableFront | keep current (high) | — |
| `AsmCommandsImpl::wprf::lineNumber` (param) | no | high | passed straight to wavetableFront | keep current (high) | — |
| `AsmCommandsImpl::wvf::waveReg` (param) | unsure | low | wrapper calls it `reg` | keep current; reg | — |
| `AsmCommandsImpl::wvf::markerReg` (param) | yes | medium | not a marker register | dstReg, secondReg | coordinated-rename |
| `AsmCommandsImpl::wvf::waveIndex` (param) | yes | medium | wrapper passes `length` | length, immVal | coordinated-rename |
| `AsmCommandsImpl::wvf::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImpl::wvfi::waveReg` (param) | unsure | low | wrapper calls it `reg` | keep current; reg | — |
| `AsmCommandsImpl::wvfi::markerReg` (param) | yes | medium | not a marker register | dstReg | coordinated-rename |
| `AsmCommandsImpl::wvfi::waveIndex` (param) | yes | medium | wrapper passes `length` | length | coordinated-rename |
| `AsmCommandsImpl::wvfi::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImplHirzel::wvfs::dummyType` (param) | no | high | enum type matches | keep current (high) | — |
| `AsmCommandsImplHirzel::wvfs::reg` (param) | no | medium | source register, single role | keep current (high) | — |
| `AsmCommandsImplHirzel::wvfs::arg` (param) | yes | medium | wrapper passes `length` | length | — |
| `AsmCommandsImplHirzel::wvfs::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImplHirzel::wvft::reg` (param) | no | medium | single source register | keep current (high) | — |
| `AsmCommandsImplHirzel::wvft::arg` (param) | yes | medium | wrapper passes `length` | length | — |
| `AsmCommandsImplHirzel::wvft::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImpl::brz::reg` (param) | no | high | comparator-target register | keep current (high) | — |
| `AsmCommandsImpl::brz::label` (param) | no | high | branch target label | keep current (high) | — |
| `AsmCommandsImpl::brz::flag` (param) | yes | high | stored in `isWaveformCmd` | isWaveformCmd | coordinated-rename |
| `AsmCommandsImpl::brz::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImpl::ssl::reg` (param) | no | medium | shifted-register operand | keep current (high) | — |
| `AsmCommandsImpl::ssl::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImpl::ssr::reg` (param) | no | medium | shifted-register operand | keep current (high) | — |
| `AsmCommandsImpl::ssr::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |
| `AsmCommandsImpl::ldiotrig::reg` (param) | no | medium | dest register | keep current (high) | — |
| `AsmCommandsImpl::ldiotrig::lineNumber` (param) | no | high | wavetableFront sink | keep current (high) | — |

## 3. Detailed findings

### AsmCommandsImpl::wvf::markerReg  [yes / medium / coordinated-rename]

Evidence:
- Header: `asm_commands_impl.hpp:43-44` —
  `wvf(AsmRegister waveReg, AsmRegister markerReg, int waveIndex, int lineNumber)`
- Caller: `asm_commands.cpp:115-119` —
  `AsmCommands::wvf(AsmRegister reg, AsmRegister dstReg, int length)`
  → `impl_->wvf(reg, dstReg, length, wavetableFrontIndex_);`
- Cervino impl: `asm_commands_impl_cervino.cpp:39-62`. The
  parameter is treated as a generic second register: `if (markerReg
  == AsmRegister::Reg(0)) { regAux = waveReg; regSrc = R0; } else
  { regAux = waveReg; regSrc = markerReg; }`. Inline comment at
  `:46`: `"Single-register form: waveReg in aux slot ..., R0 in src
  slot"` — describes a generic two-register form, no marker
  semantics.
- Hirzel impl: `asm_commands_impl_hirzel.cpp:43-63`. Same generic
  pattern — branches on whether `markerReg == R0` to emit the
  single-reg variant `WVFE` vs two-reg variant `WVF`. Comment at
  `:53`: `"Two-register form, same as Cervino"`.
- The wrapper `AsmCommands::wvf(reg, dstReg, length)` in
  `asm_commands.cpp:115` and the tablular comment at `asm_commands.cpp:95`
  (`regAux = reg1; regSrc = reg2`) treat the second register
  positionally as `dstReg` / `reg2`, not as a marker.

Interpretation:
- The parameter is used as a generic second register that, when
  non-R0, occupies the source slot of the encoded WVF instruction.
  No code path treats this register as containing marker bits or
  references markers. The wrapper layer above already calls it
  `dstReg`. The name `markerReg` is inherited from the public-ABI
  family of WVF opcodes that historically encoded a marker
  register, but at this internal layer the function only sees an
  abstract register operand.

Judgement:
- The name `markerReg` overstates the parameter's semantics; it is
  just a second register operand whose `R0`-vs-non-`R0` value
  selects the encoding form. Misnomer.

Proposals:
- dstReg          (medium) — matches the wrapper's parameter name.
- secondReg       (low)
- markerReg       (low) — keep current; the name does describe how
  some callers use it.

Cross-reference:
- All three declarations (base + Cervino + Hirzel overrides) must
  be renamed together — same vtable slot. `coordinated-rename`.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:43-44, 73-74, 100-101
- defined: src/asm/asm_commands_impl_cervino.cpp:39-62; src/asm/asm_commands_impl_hirzel.cpp:43-63
- caller: src/asm/asm_commands.cpp:115-119

### AsmCommandsImpl::wvf::waveIndex  [yes / medium / coordinated-rename]

Evidence:
- Header: `asm_commands_impl.hpp:44` — `int waveIndex`.
- Caller: `asm_commands.cpp:115` — wrapper parameter is
  `int length`, passed as the third argument to `impl_->wvf(reg,
  dstReg, length, ...)`.
- Cervino impl: `asm_commands_impl_cervino.cpp:55-58` — comment
  `"waveIndex goes in outputs (not immediates)..."`, then
  `result.assembler.outputs.emplace_back(waveIndex);`.
- Hirzel impl: `asm_commands_impl_hirzel.cpp:59` —
  `result.assembler.outputs.emplace_back(waveIndex);`.
- The same positional argument is called `arg` in `wvfs`/`wvft`
  (Hirzel impls), `length` in the wrapper, and `waveIndex` here.

Interpretation:
- The name in the wrapper layer (the public API) is `length`,
  meaning the duration in samples that the waveform plays. Inside
  the impl the value is just emplaced into the encoded
  instruction's `outputs` field with no semantic interpretation;
  the impl never treats it as an index into a waveform table. The
  three sibling impls disagree on the parameter name (`waveIndex`,
  `arg`, `length`).

Judgement:
- The impl-side name `waveIndex` does not describe what the value
  is; the wrapper-side `length` does. Misnomer.

Proposals:
- length    (medium) — aligns with wrapper.
- immVal    (low)    — neutral encoding-side name.
- keep current (low)

Cross-reference:
- Coordinated rename across base + Cervino + Hirzel overrides.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:44, 76, 101
- defined: src/asm/asm_commands_impl_cervino.cpp:39-62; src/asm/asm_commands_impl_hirzel.cpp:43-63
- caller: src/asm/asm_commands.cpp:115-119

### AsmCommandsImpl::wvfi::markerReg  [yes / medium / coordinated-rename]

Evidence:
- Header: `asm_commands_impl.hpp:45-46`.
- Cervino impl: `asm_commands_impl_cervino.cpp:66-82` — the only
  use of `markerReg` is `if (markerReg != AsmRegister::Reg(0))
  throw ...;`. The instruction never stores it in any register
  slot; only `regAux = waveReg; regSrc = R0;`.
- Hirzel impl: `asm_commands_impl_hirzel.cpp:67-71` — the
  parameter is unnamed (`wvfi(AsmRegister, AsmRegister, ...)`) and
  the function unconditionally throws.
- Caller: `asm_commands.cpp:122` — wrapper calls it `dstReg`.

Interpretation:
- This parameter is only ever validated (must be R0 on Cervino) or
  ignored (Hirzel always throws). It is not used as a marker
  register. The name is inherited from `wvf` for symmetry but does
  not describe any actual function in `wvfi`.

Judgement:
- Misnomer for the same reason as `wvf::markerReg`.

Proposals:
- dstReg     (medium)
- keep current (low)

Cross-reference:
- Same coordinated rename group as `wvf::markerReg` if they should
  stay aligned.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:45-46, 75-76, 103
- defined: src/asm/asm_commands_impl_cervino.cpp:66-82; src/asm/asm_commands_impl_hirzel.cpp:67-71

### AsmCommandsImpl::wvfi::waveIndex  [yes / medium / coordinated-rename]

Evidence:
- Same situation as `wvf::waveIndex`. Cervino emits
  `outputs.emplace_back(waveIndex);` at
  `asm_commands_impl_cervino.cpp:78` (comment: `"outputs, not
  immediates (child order)"`); Hirzel throws.
- Wrapper passes its own `length` parameter
  (`asm_commands.cpp:122,126`).

Interpretation, Judgement, Proposals — identical to
`wvf::waveIndex` block above.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:46, 76, 103
- defined: src/asm/asm_commands_impl_cervino.cpp:66-82
- caller: src/asm/asm_commands.cpp:122-126

### AsmCommandsImpl::wvf::waveReg  [unsure / low / —]

Evidence:
- Header: `asm_commands_impl.hpp:43-44` — first reg is `waveReg`,
  second is `markerReg`.
- Cervino impl uses `regAux = waveReg;` whether or not `markerReg`
  is R0 (`asm_commands_impl_cervino.cpp:47,51`). Hirzel impl uses
  `regSrc = waveReg;` in both branches
  (`asm_commands_impl_hirzel.cpp:51,55`).
- Wrapper calls it `reg` (`asm_commands.cpp:115`).

Interpretation:
- The parameter does carry a "wave" register in the public-API
  sense (the register holding the wave-table index/handle), but
  the wrapper layer one level up just calls it `reg`. There is no
  contradiction with the name, only a mild specificity mismatch.

Judgement:
- Unsure: the name is plausibly correct given the WVF instruction
  family's conventional semantics, but no positive in-tree
  evidence anchors it.

Proposals:
- keep current  (medium)
- reg           (low) — would align with wrapper / siblings.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:43, 73, 100
- defined: src/asm/asm_commands_impl_cervino.cpp:39-62; src/asm/asm_commands_impl_hirzel.cpp:43-63

### AsmCommandsImpl::brz::flag  [yes / high / coordinated-rename]

Evidence:
- Header: `asm_commands_impl.hpp:51-52` —
  `brz(AsmRegister reg, const std::string& label, bool flag, int lineNumber)`.
- Cervino impl: `asm_commands_impl_cervino.cpp:101-111`. Only
  use: `result.isWaveformCmd = flag;  // directly stored, not
  computed from opcode` (line 109).
- Hirzel impl: `asm_commands_impl_hirzel.cpp:108-125`. Only use:
  `result.isWaveformCmd = flag;` (line 123).
- Caller: `asm_commands.cpp:166-170` —
  `AsmCommands::brz(reg, label, flag) ... return impl_->brz(reg,
  label, flag, ...);`. The wrapper *also* uses the name `flag`,
  passed straight through.
- Sibling free functions in same TU: `brnz`/`brgz` likewise have
  `bool flag` parameters that go directly to
  `result.isWaveformCmd = flag;` (`asm_commands.cpp:188,207`).
- Cross-batch: identical pattern noted in
  `09_prefetch.md`/`09_prefetch_part2.md` for several other
  `bool flag` parameters that mean `isWaveformCmd`.

Interpretation:
- The parameter's only effect is to set the `isWaveformCmd`
  field of the produced `AsmList::Asm`. There is no other use, no
  other branch on it. The name `flag` is the textbook generic
  boolean name with no semantic content.

Judgement:
- Misnomer. The parameter is the value of `isWaveformCmd`.

Proposals:
- isWaveformCmd  (high)
- keep current   (low)

Cross-reference:
- Coordinated rename: base declaration + Cervino + Hirzel
  overrides + the wrapper `AsmCommands::brz/brnz/brgz/br`
  parameters in `asm_commands.cpp:162-208`.
- Same family as the "`flag` actually means `isWaveformCmd`" finds
  flagged in batch 9.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:51-52, 81-82, 111-112
- defined: src/asm/asm_commands_impl_cervino.cpp:101-111; src/asm/asm_commands_impl_hirzel.cpp:108-125
- caller: src/asm/asm_commands.cpp:162-170, 173-208

### AsmCommandsImplHirzel::wvfs::arg  [yes / medium / —]

Evidence:
- Header: `asm_commands_impl.hpp:48` — `int arg`.
- Hirzel impl: `asm_commands_impl_hirzel.cpp:75-90`. Only use:
  `result.assembler.outputs.emplace_back(arg);` with comment
  `"// child[2]: val (20-bit, after registers)"`.
- Cervino impl: parameters all unnamed
  (`asm_commands_impl_cervino.cpp:86`) — function throws.
- Caller: `asm_commands.cpp:129,137` — wrapper parameter is
  `int length`, passed in this position.

Interpretation:
- The wrapper-layer name is `length`; the impl-layer name is the
  generic `arg`. Same situation as `wvf::waveIndex`, but on a
  function whose Cervino override throws and so doesn't anchor the
  name.

Judgement:
- Generic `arg` does not describe the value; wrapper says `length`.
  Misnomer.

Proposals:
- length        (medium)
- keep current  (low)

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:48, 77, 106
- defined: src/asm/asm_commands_impl_hirzel.cpp:75-90
- caller: src/asm/asm_commands.cpp:129-137

### AsmCommandsImplHirzel::wvft::arg  [yes / medium / —]

Evidence:
- Header: `asm_commands_impl.hpp:49` — `int arg`.
- Hirzel impl: `asm_commands_impl_hirzel.cpp:94-104` — only use
  `outputs.emplace_back(arg);` (line 100).
- Cervino impl: parameters unnamed, throws
  (`asm_commands_impl_cervino.cpp:94`).
- Caller: `asm_commands.cpp:140` — wrapper parameter is `length`.

Interpretation, Judgement, Proposals — same as `wvfs::arg`.

Locations consulted:
- declared: include/zhinst/asm/asm_commands_impl.hpp:49, 79, 108
- defined: src/asm/asm_commands_impl_hirzel.cpp:94-104
- caller: src/asm/asm_commands.cpp:140-141

## 4. Symbols inspected and judged routinely fine

- `AsmCommandsImpl::getInstance::deviceType` — sole use is
  `static_cast<int>(deviceType)`; type matches enum
  `AwgDeviceType`; name is clear.
- `getInstance` locals `val`, `shifted`, `mask` — each has a
  one-line role obvious from the surrounding bit-test
  (`asm_commands_impl.cpp:16-26`); `mask` is even adjacent to a
  comment naming it as the "Hirzel device set bitmask".
- All `lineNumber` parameters across the 13 virtuals — each is
  forwarded straight into `result.wavetableFront = lineNumber;`
  inside its respective impl (verified at the cited lines in §3).
  The wrapper passes `wavetableFrontIndex_` here. The name is
  consistent with the value's role across both sibling impls and
  is not semantically wrong (a wavetable-front index is in fact
  derived from a SeqC source line number; `wavetableFrontIndex_`
  is a separate naming question for the AsmCommands batch).
- `AsmCommandsImpl::wprf::lineNumber`, `wwvfq::lineNumber` — only
  use is `result.wavetableFront = lineNumber;` in both impls; ditto
  above.
- `AsmCommandsImplHirzel::wvfs::dummyType` — typed
  `Assembler::PlayDummyType`, used as `(static_cast<int>(dummyType)
  != 0) ? 1 : 0` to derive a 1-bit immediate. Name matches type.
- `AsmCommandsImplHirzel::wvfs::reg`, `wvft::reg` — single
  source-register operand; assigned to `regSrc`. Generic but not
  misleading.
- `AsmCommandsImpl::brz::reg`, `brz::label` — branch register and
  label name; used exactly as named.
- `AsmCommandsImpl::ssl::reg`, `ssr::reg`, `ldiotrig::reg` — each
  the operand register for a single-register instruction; assigned
  to `regDst` (and `regSrc` on Cervino's `ssl/ssr` two-slot form).
  Generic but accurate.
- All Cervino-side parameters for `wvfs(PlayDummyType, AsmRegister,
  int, int)` and `wvft(AsmRegister, int, int)` are **unnamed** in
  the source, so there is no name to flag.

## 5. Coverage

Fully covered:
- All four files in scope: header + three impl translation units.
- Every parameter of every method declaration and definition
  (where named).
- Locals in the only function that has any (`getInstance`).

Out of scope per §3:
- Type names `AsmCommandsImpl`, `AsmCommandsImplCervino`,
  `AsmCommandsImplHirzel` — present in `nm` typeinfo/vtable.
- All thirteen virtual method names + destructor + the static
  factory `getInstance` — present in `nm`.
- The class has no data members (abstract base + two empty derived
  classes), and no namespace constants/enums/macros are defined in
  these files.

Not in scope (different batch):

- The cross-batch-arbitration entries from `38_play_config.md`
  (`isFourChannelBool`/`isHoldMode`/`holdCount`,
  `PlayConfig::now`/`is4Channel`/`rate`) name
  `AsmCommands::genPlayConfig` as the producer side. That method
  lives in `asm_commands.cpp:960` (verified via `nm` —
  `zhinst::AsmCommands::genPlayConfig(...)`), not in any file in
  this batch's scope. The play_config report's "batch 49"
  attribution is incorrect; arbitration must be done against the
  batch that owns `asm_commands.cpp` / `AsmCommands` (a separate,
  not-yet-completed batch). Recording here so synthesis knows: the
  three flagged producer-side parameters were not inspected by
  this batch and remain `cross-batch-arbitration` against
  whichever batch covers `AsmCommands::genPlayConfig`.

Hirzel/Cervino selector pattern:
- The pimpl/factory `AsmCommandsImpl::getInstance` is the explicit
  Cervino-vs-Hirzel selector for this whole subsystem. Its single
  parameter `deviceType` is correctly typed and named; no `flag`-
  shaped boolean appears here. The `bool flag` parameter on `brz`
  is *not* a Hirzel-vs-Cervino selector — it is the
  `isWaveformCmd` value (separately flagged above). No additional
  Hirzel/Cervino-selector misnomer found in this batch.
