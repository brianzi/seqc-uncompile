# Batch 32 — frontend_lowering

## 1. Files considered

- `reconstructed/include/zhinst/ast/frontend_lowering.hpp`
- `reconstructed/src/ast/frontend_lowering.cpp`

The pair only declares two stack-local structures (`FrontendLoweringContext`,
`FrontendLoweringState`) and their defaulted destructors. **All meaningful
use sites of the fields are in other TUs** — primarily
`src/ast/seqc_ast_nodes_evaluate.cpp` and `src/codegen/compiler.cpp`. Those use sites
were consulted (not edited) to judge the field names.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `FrontendLoweringContext` (type) | no | high | dtor mangled in nm | keep current | not-misnomer |
| `FrontendLoweringContext::messages` | no | medium | always carries CompilerMessageCollection | keep current | — |
| `FrontendLoweringContext::asmCommands` | no | medium | always carries AsmCommands subsystem | keep current | — |
| `FrontendLoweringContext::customFunctions` | no | medium | always carries CustomFunctions subsystem | keep current | — |
| `FrontendLoweringContext::waveformGen` | no | medium | always carries WaveformGenerator subsystem | keep current | — |
| `FrontendLoweringContext::wavetable` | no | medium | always carries WavetableFront subsystem | keep current | — |
| `FrontendLoweringContext::channelGrouping` | yes | medium | actually loop-unroll iteration limit | `loopUnrollLimit`, `unrollLimit`, keep current | coordinated-rename |
| `FrontendLoweringContext::~FrontendLoweringContext` (no params) | — | — | trivial | — | — |
| `FrontendLoweringState` (type) | no | high | dtor mangled in nm | keep current | not-misnomer |
| `FrontendLoweringState::result` | no | medium | lowered AST root, set/read by lower() | keep current | — |
| `FrontendLoweringState::pad10_` | yes | high | name says padding; used as flag | `inFunctionDef_`, `pad10_` (keep) | — |
| `FrontendLoweringState::strings` | yes | medium | label stack, not generic strings | `returnLabels`, `labelStack`, keep current | — |
| `FrontendLoweringState::inLoop_` | unsure | medium | also repurposed as inFunction flag | `inLoopOrFunction_`, keep current | — |
| `FrontendLoweringState::inSwitch_` | no | high | gates SeqCCaseEntry, set by SeqCSwitchCase | keep current | not-misnomer |
| `FrontendLoweringState::~FrontendLoweringState` (no params) | — | — | trivial | — | — |

## 3. Detailed findings

### FrontendLoweringContext (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so`:
  `0x1233b0 t zhinst::FrontendLoweringContext::~FrontendLoweringContext()`
- The type appears as parameter type in many other mangled symbols, e.g.
  `(anonymous namespace)::invertBool(..., zhinst::FrontendLoweringContext&)`
  `zhinst::SeqCAssign::evaluate(..., zhinst::FrontendLoweringContext&, ...)`

Interpretation:
- Itanium mangling encodes the fully-qualified type name. Per RULES §3,
  this is tier-1 authoritative.

Judgement:
- Not a misnomer; type name is taken from the binary symbol table.

Proposals:
- keep current (high)

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:46`

---

### FrontendLoweringContext::channelGrouping  [yes / medium / coordinated-rename]

Evidence:
- `frontend_lowering.hpp:52` `int32_t channelGrouping;   // +0x48`
- `compiler.cpp:681` `context.channelGrouping = channelGrouping;`
  — fed in from `FrontEndLoweringFacade::lower()` parameter.
- `seqc_ast_nodes_evaluate.cpp:8202`
  `if (iterCount > ctx.channelGrouping) {                   // @0x21eb1e`
  (in SeqCForLoop loop-unroll code; error 0x7b on overshoot).
- `seqc_ast_nodes_evaluate.cpp:8501` same pattern in SeqCDoWhile.
- `seqc_ast_nodes_evaluate.cpp:8779` same pattern in SeqCRepeat
  (`if (countInt > ctx.channelGrouping)`).
- `seqc_ast_nodes_evaluate.cpp:10010` same pattern in SeqCWhile
  (`if ((iterCount - 1) > ctx.channelGrouping)`).
- Header comment in evaluate.cpp:8077, 8340, 8667 explicitly says
  "iteration count exceeding ctx.channelGrouping (error 0x7b)".

Interpretation:
- The field is read at four loop-unroll sites as an upper bound on
  iteration count. None of the read sites involve channels or grouping.
  At the entry point the value comes from the same call-stack source as
  `AWGCompilerConfig::channelGrouping` and `FrontEndLoweringFacade::
  lower::channelGrouping`, both of which were independently flagged as
  loop-unroll limits in batches 23 and 07.

Judgement:
- Misnomer: the value is the unroll iteration limit, not a
  channel-grouping configuration.

Proposals:
- `loopUnrollLimit` (medium) — most descriptive at use site
- `unrollLimit` (low) — shorter form
- keep current (low) — only justified for in-lockstep stability

Cross-reference:
- `AWGCompilerConfig::channelGrouping` in batch 23 (already flagged
  yes / medium).
- `FrontEndLoweringFacade::lower::channelGrouping` parameter in
  batch 07 (already flagged yes / medium / coordinated-rename).
- All three should be renamed in lockstep.

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:52`
- written: `src/codegen/compiler.cpp:681`
- read:    `src/ast/seqc_ast_nodes_evaluate.cpp:8202,8501,8779,10010`

---

### FrontendLoweringState (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so`:
  `0x1c2190 t zhinst::FrontendLoweringState::~FrontendLoweringState()`
- Appears as parameter type in many mangled symbols (every
  `Seq*::evaluate(..., FrontendLoweringState&)`).

Interpretation:
- Tier-1 authoritative per RULES §3.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:89`

---

### FrontendLoweringState::pad10_  [yes / high / —]

Evidence:
- `frontend_lowering.hpp:91`
  `uint64_t pad10_{};    // +0x10 (zeroed in lower())`
  with header comment line 68 `+0x10  8   (zeroed — possibly bool or padding)`.
- `seqc_ast_nodes_evaluate.cpp:9799`
  `state.pad10_ = 1;   // 0x20b8ae: [r13+0x10] = 1 — "inside function def"`
- `seqc_ast_nodes_evaluate.cpp:9811`
  `state.pad10_ = 0;   // 0x20b954: [rax+0x10] = 0`
  (paired with the above; cleared after function-body evaluate returns.)
- `seqc_ast_nodes_evaluate.cpp:7955`
  `bool usePad10 = (state.pad10_ == 1);` (in SeqCSwitchCase).
- `seqc_ast_nodes_evaluate.cpp:8015,8027,8032,9265`
  read sites that branch on `state.pad10_`, e.g.
  `if (!state.pad10_) { ... }` and
  `if (static_cast<uint8_t>(state.pad10_) == 1) { ... override hasError_ ... }`.

Interpretation:
- The field is a semantically-meaningful flag (set on entering a
  user-function body and consulted by switch/case error-aggregation
  paths). It is not padding — the name is actively misleading and
  the existing inline comment already speculates "possibly bool or
  padding".

Judgement:
- Misnomer: the name advertises padding, the code uses it as a flag.

Proposals:
- `inFunctionDef_` (medium) — matches the only set-site comment
- `inFunctionBody_` (low) — synonym
- `pad10_` keep current (low) — only if the auditor remains
  uncertain about the semantic; would force readers to keep
  re-deriving the meaning.

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:91`
- set:     `src/ast/seqc_ast_nodes_evaluate.cpp:9799,9811`
- read:    `src/ast/seqc_ast_nodes_evaluate.cpp:7955,8015,8027,8032,9265`

---

### FrontendLoweringState::strings  [yes / medium / —]

Evidence:
- `frontend_lowering.hpp:92` `std::vector<std::string> strings; // +0x18`
- `seqc_ast_nodes_evaluate.cpp:5792` (header comment)
  "Common tail: emits br(state.strings.back()) if inside a function scope"
- `seqc_ast_nodes_evaluate.cpp:6065-6067`
  `if (!state.strings.empty()) { ...br(..., state.strings.back(), false); }`
  — back element used as a branch target label.
- `seqc_ast_nodes_evaluate.cpp:6341-6384`
  `// newLabel("ret"), push to state.strings`
  `state.strings.push_back(std::move(retLabel));`
- `seqc_ast_nodes_evaluate.cpp:6686-6688`
  `if (!state.strings.empty()) { lastLabel = move(state.strings.back());
   state.strings.pop_back(); }`
  — used as a stack via push/back/pop_back of "ret" labels.

Interpretation:
- All observed access patterns treat the vector as a LIFO stack of
  labels (specifically "ret" labels generated by SeqCFunctionDef and
  consumed by SeqCReturnStatement). The strings are label names used
  for `br` emission. The generic name `strings` does not convey
  this stack-of-labels role.

Judgement:
- Misnomer: name is too generic for the actual stack-of-return-labels
  semantics.

Proposals:
- `returnLabels` (medium) — matches the dominant usage site
- `labelStack` (low) — broader if other label uses are later found
- keep current (low) — only acceptable if the auditor wishes to be
  conservative about scope breadth.

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:92`
- used:    `src/ast/seqc_ast_nodes_evaluate.cpp:5792,6065,6067,6341,6384,6686,6687,6688`

---

### FrontendLoweringState::inLoop_  [unsure / medium / —]

Evidence:
- `frontend_lowering.hpp:93`
  `uint8_t inLoop_{};  // +0x30 (checked by break/continue)`
- `seqc_ast_nodes_evaluate.cpp:6581-6584` (in SeqCFunctionDef body):
  `// NOTE: state.inLoop_ at +0x30 is repurposed here as "inFunction" flag.`
  `// Binary reads/writes state+0x30 for this purpose.`
  `uint8_t savedInLoop = state.inLoop_; state.inLoop_ = 1;`
- `seqc_ast_nodes_evaluate.cpp:6600,6609,6640,6646` save/restore
  pattern in SeqCFunctionDef.
- `seqc_ast_nodes_evaluate.cpp:6796`
  `if (state.inLoop_) { ... }` — checked inside function-body code path
  (not break/continue).

Interpretation:
- Source comment already flags that the field is overloaded: it is
  set both by loop bodies (break/continue) and by function-body
  evaluation. If the reconstructed "inFunction repurpose" reading is
  correct, the name is incomplete; if the binary actually treats
  this as a single "inside-loop-or-function-scope" flag, the name
  partially fits but understates the second role.
- Resolving which interpretation matches the binary requires dynamic
  inspection (GDB) of break/continue paths in code that is also
  inside a function — out of scope for this read-only audit.

Judgement:
- Unsure: name is at minimum incomplete given the function-def use
  site, but the auditor cannot decide between "rename to capture
  both roles" and "two distinct flags incorrectly merged".

Proposals:
- `inLoopOrFunction_` (low) — captures the observed dual use
- keep current (low) — if the function-def repurpose turns out to
  be a reconstruction artifact
- (rename pending GDB confirmation)

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:93`
- used:    `src/ast/seqc_ast_nodes_evaluate.cpp:6581,6583,6584,6600,6609,6640,6646,6796`

---

### FrontendLoweringState::inSwitch_  [no / high / not-misnomer]

Evidence:
- `frontend_lowering.hpp:94`
  `uint8_t inSwitch_{};  // +0x31 (checked by SeqCCaseEntry)`
- `seqc_ast_nodes_evaluate.cpp:7180`
  `if (!state.inSwitch_) { /* reject case outside switch */ }`
  (SeqCCaseEntry::evaluate).
- `seqc_ast_nodes_evaluate.cpp:7549-7551` (SeqCSwitchCase::evaluate):
  `// ---- Save and set state.inSwitch_ ---- `
  `const bool savedInSwitch = state.inSwitch_; state.inSwitch_ = true;`
- `seqc_ast_nodes_evaluate.cpp:7567,7581,7602,7919,7939,8038,8052`
  consistent restore on every exit path of SeqCSwitchCase.

Interpretation:
- The flag is set exactly when entering a switch/case body and
  consulted exactly to validate `case`/`default` placement. Usage
  unambiguously matches the name across many sites with no
  contradicting context (RULES §4d, tier 4).

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:94`
- used:    `src/ast/seqc_ast_nodes_evaluate.cpp:7180,7549,7550,7551,7567,7581,7602,7919,7939,8038,8052`

---

### FrontendLoweringState::result  [no / medium / —]

Evidence:
- `frontend_lowering.hpp:90`
  `std::shared_ptr<Node> result;  // +0x00 (lowered AST root)`
- `compiler.cpp:656,661,692,694`
  `// Returns {state.result (shared_ptr<Node>), evaluate_output ...}`
  `result.astResult = std::move(state.result);`
- Header comments lines 76-82 trace the lowered-AST-root identity from
  the binary disassembly (`Compiler+0x28` is `shared_ptr<Node> ast_`).

Interpretation:
- All read/write sites use this as the lowered AST root produced by
  the dispatch in `lower()`. The name `result` is generic but the
  containing struct (`FrontendLoweringState`) supplies enough context
  that "result of lowering" is the obvious reading.

Judgement:
- Not a misnomer in context, though `loweredAst` would be more
  descriptive if a rename pass touches this struct.

Proposals:
- keep current (medium)
- `loweredAst` (low) — minor readability improvement

Locations consulted:
- declared: `include/zhinst/ast/frontend_lowering.hpp:90`
- used:    `src/codegen/compiler.cpp:656,661,692,694`

## 4. Symbols inspected and judged routinely fine

- `FrontendLoweringContext::messages` — non-owning pointer to a
  `CompilerMessageCollection`; used via `ctx.messages->errorMessage(...)`
  and `ctx.messages->setLineNr(...)` at >100 sites in
  `seqc_ast_nodes_evaluate.cpp`. Name matches role.
- `FrontendLoweringContext::asmCommands` — owning shared_ptr to
  `AsmCommands`; used as `ctx.asmCommands->asmZero(...)`,
  `ctx.asmCommands->addi(...)` etc., dozens of sites. Name fits.
- `FrontendLoweringContext::customFunctions` — owning shared_ptr to
  `CustomFunctions`; consistent with type.
- `FrontendLoweringContext::waveformGen` — owning shared_ptr to
  `WaveformGenerator`; used as `ctx.waveformGen->eval("rect", ...)`,
  `ctx.waveformGen->eval("scale", ...)` at multiple sites. Name fits.
- `FrontendLoweringContext::wavetable` — owning shared_ptr to
  `WavetableFront`; used as `ctx.wavetable->newEmptyWaveform`,
  `ctx.wavetable->setLineNr`, `ctx.wavetable->getWaveformSampleLength`,
  many sites. Name fits.
- `FrontendLoweringContext::~FrontendLoweringContext()` — no parameters.
- `FrontendLoweringState::~FrontendLoweringState()` — no parameters.
- Both destructors' names are mangled in `nm`, hence excluded from
  rename per RULES §3.

## 5. Coverage

- **Fully covered:**
  - Both type names (`FrontendLoweringContext`, `FrontendLoweringState`)
    confirmed authoritative via `nm --demangle`.
  - All 6 fields of `FrontendLoweringContext`: judged.
  - All 5 fields of `FrontendLoweringState`: judged.
  - Both destructors: no parameters; nothing to audit beyond the
    type-name confirmation.
- **Deferred:**
  - `FrontendLoweringState::inLoop_` — left `unsure`; deciding between
    "keep" and "rename to capture function-def repurpose" requires GDB
    confirmation that both code paths actually run, which is out of
    scope for this read-only audit.
  - The free function `(anonymous namespace)::constWaveform(int, double,
    FrontendLoweringContext&)` mentioned in the header comment block
    is **not** declared in this header (it is internal to a different
    .cpp). Its parameter names belong to whichever batch covers
    `seqc_ast_nodes_evaluate.cpp` (or its anon-namespace helpers) and
    are not in scope here.
- **Not covered (out of scope per RULES §2/§3):**
  - The two destructor *names* — found in `nm` symbol table as
    fully-qualified mangled symbols, excluded by §3.
  - The two type *names* (`FrontendLoweringContext`,
    `FrontendLoweringState`) — same, excluded by §3.
  - All numerous helper functions and `Seq*::evaluate` overloads that
    take `FrontendLoweringContext&` / `FrontendLoweringState&` are in
    other batches (their .cpp files), not these two files.
