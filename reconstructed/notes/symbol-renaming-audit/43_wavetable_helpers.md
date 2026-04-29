# Batch 43 — wavetable_helpers

## 1. Files considered

- `reconstructed/include/zhinst/wavetable_helpers.hpp`

Call-site context (read-only, for evidence):
- `reconstructed/src/wavetable_ir.cpp`
- `reconstructed/src/wavetable_front.cpp`
- `reconstructed/src/wavetable_manager_front.cpp`

Symbol-table check:
- `nm --demangle _seqc_compiler.so` →
  `0000000000 2a0fd0 t zhinst::(anonymous namespace)::getUniqueName(std::__1::basic_string<...> const&, int, int)`

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `detail::getUniqueName::base` | no | medium | seed name for unique id | keep current (medium) | not-misnomer |
| `detail::getUniqueName::index` | yes | medium | always source line number | `lineNr` (medium), `lineIdx` (medium), keep current (low) | — |
| `detail::getUniqueName::counter` | no | high | always waveformCounter_ value | keep current (high) | not-misnomer |

Out of scope (per §3, in symbol table):
- `detail::getUniqueName` — function name authoritative.

## 3. Detailed findings

### detail::getUniqueName::base  [no / medium / not-misnomer]

Evidence:
- include/zhinst/wavetable_helpers.hpp:31  `oss << "__" << base << "_" << index << "_" << counter;`
- src/wavetable_ir.cpp:540,546  `std::string fillerName = "filler"; … getUniqueName(fillerName, lineIdx, counter);`
- src/wavetable_front.cpp:215  `getUniqueName(funName, baseIndex, counter);` where `funName` is the user-facing waveform-generator function name.
- src/wavetable_manager_front.cpp:59,66  parameter `name` (a waveform name) → `getUniqueName(name, baseIdx, counter);`
- src/wavetable_manager_front.cpp:258  `getUniqueName(srcPtr->name, baseIdx, counter);` (source waveform's name).

Interpretation:
- The first positional argument is consistently a base/seed string (literal `"filler"`, a function name, or an existing waveform name) that becomes the human-readable root of the generated unique name. Format places it between the `"__"` prefix and the numeric suffixes.

Judgement:
- `base` accurately reflects the role: it is the seed string the unique name is built around.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/wavetable_helpers.hpp:28
- used:     src/wavetable_ir.cpp:546; src/wavetable_front.cpp:215; src/wavetable_manager_front.cpp:66,258

### detail::getUniqueName::index  [yes / medium / —]

Evidence:
- include/zhinst/wavetable_helpers.hpp:31  `oss << "__" << base << "_" << index << "_" << counter;`
- src/wavetable_ir.cpp:544–546  `int lineIdx = manager_->lineNr_; … getUniqueName(fillerName, lineIdx, counter);` — caller renames it `lineIdx`, sourced from `manager_->lineNr_`.
- src/wavetable_front.cpp:211–215  `int baseIndex = manager_->lineNr_; … getUniqueName(funName, baseIndex, counter);` — caller renames it `baseIndex`, also sourced from `manager_->lineNr_`.
- src/wavetable_manager_front.cpp:62,66  `int baseIdx = lineNr_; … getUniqueName(name, baseIdx, counter);`
- src/wavetable_manager_front.cpp:254,258  `int baseIdx = lineNr_; … getUniqueName(srcPtr->name, baseIdx, counter);`
- Adjacent comment src/wavetable_ir.cpp:541-543: "edx = manager_->lineNr_, ecx = manager_->waveformCounter_".

Interpretation:
- At every call site the value bound to `index` is the manager's `lineNr_` field. None of the call sites pass an array index, waveform index, table index, or any other "index" in the conventional sense — the value is always a SeqC source line number. Two call sites already rename their local copy to `baseIndex`/`baseIdx` (a hedge), one to `lineIdx` (closer to the truth).

Judgement:
- `index` is generic and misleading: the value is specifically a source line number used as a disambiguating suffix component, not an index into anything.

Proposals:
- `lineNr`  (medium)   — matches `WavetableManager::lineNr_`, the actual source field; clearest provenance.
- `lineIdx` (medium)   — already used as the local name in `wavetable_ir.cpp:544`.
- keep current (`index`) (low) — only justified if the parameter's role were truly opaque, which the call sites contradict.

Cross-reference:
- `lineNr_` is a field of `WavetableManager` (likely batch 46 `wavetable_ir`/manager). If batch 46 finds `lineNr_` is itself a misnomer (e.g. it carries something other than a line number), this proposal should follow that decision. Not flagged `cross-batch-arbitration` because the inconsistency here is parameter-vs-field (he-said/she-said is a tie that the call sites resolve in favour of `lineNr_`).

Locations consulted:
- declared: include/zhinst/wavetable_helpers.hpp:28
- used:     src/wavetable_ir.cpp:544–546; src/wavetable_front.cpp:211–215; src/wavetable_manager_front.cpp:62,66,254,258

### detail::getUniqueName::counter  [no / high / not-misnomer]

Evidence:
- include/zhinst/wavetable_helpers.hpp:31  `oss << … << "_" << counter;`
- src/wavetable_ir.cpp:545  `int counter = manager_->waveformCounter_++;`
- src/wavetable_front.cpp:212–213  `int counter = manager_->waveformCounter_; manager_->waveformCounter_ = counter + 1;`
- src/wavetable_manager_front.cpp:63–64  `int counter = waveformCounter_; waveformCounter_ = counter + 1;`
- src/wavetable_manager_front.cpp:255–256  same pattern.

Interpretation:
- All four call sites bind `counter` to the post-incremented `waveformCounter_` field. Every caller also names the local `counter`. The field name (manager-owned) and the parameter agree, and the value is in fact a monotonically increasing counter used as the final disambiguator.

Judgement:
- `counter` is correct.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/wavetable_helpers.hpp:28
- used:     src/wavetable_ir.cpp:545; src/wavetable_front.cpp:212; src/wavetable_manager_front.cpp:63,255

## 4. Symbols inspected and judged routinely fine

(None beyond the three parameters above; the file contains exactly one
function, no fields/enums/constants/macros.)

## 5. Coverage

- **Fully covered:** all in-scope symbols in `wavetable_helpers.hpp` —
  the three parameters of `detail::getUniqueName`. Every call site in
  the recon was read.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - `detail::getUniqueName` (function name) — present in the binary
    symbol table at `0x2a0fd0` (`zhinst::(anonymous namespace)::getUniqueName`),
    excluded per §3.
  - The `detail` namespace itself — not a project type/class and only a
    recon-side packaging device for what was an anon-namespace TU-local
    helper; not in any of the in-scope categories of §2.
