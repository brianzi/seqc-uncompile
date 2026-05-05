# Batch 19b — resources supplementary (.cpp definitions)

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 9 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 5;
> B3 (already resolved during Phase D/R): 4;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Sub-batch 19b: the four supplementary `.cpp` files split out of
`resources.cpp`. The header (`resources.hpp`) and the main
`resources.cpp` are owned by sub-batch 19a — fields declared in the
header (e.g. `Function::name`, `Function::signature`, `parent_`,
`parentWeak_`, `returnType_`, `returnValue_`, `returnReg_`,
`variables_`, `functions_`, `usedSampleRate_`, `functionStorage_`,
`functionPtr_`) are **out of scope here**, even though they are read or
written in these `.cpp` files.

In-scope symbols are limited to:
- Parameters of methods/free functions whose **definitions** live in
  these four files.
- Locals declared in those definitions.
- Named constants declared inside those definitions.
- `extern` redeclarations made in these files (where they shadow or
  document a constant defined elsewhere).

## 1. Files considered

- `reconstructed/src/runtime/resources_function.cpp` — `Resources::Function`
  method definitions and a few `Resources` free-store helpers
  (`functionExistsInScope`, `functionExists`, `getFunction`,
  `getPossibleFunctions`, `addFunction`, `getRegister`).
- `reconstructed/src/runtime/resources_static_global.cpp` — `StaticResources`
  ctor, dtor, `getVariable`. (`GlobalResources` ctor/dtor live in
  `global_resources.cpp`.)
- `reconstructed/src/runtime/static_resources.cpp` — `StaticResources::init`
  only.  All other `StaticResources` methods are in
  `resources_static_global.cpp`.
- `reconstructed/src/runtime/global_resources.cpp` — `GlobalResources` ctor,
  dtor, TLS storage definitions for `regNumber`, `labelIndex`,
  `random[313]`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| **Resources::Function::Function (4-arg @0x1eaa00)** | | | | | |
| `Function::Function::name` (param) | no | medium | mirrors header field | keep current | not-misnomer |
| `Function::Function::sig` (param) | no | medium | mirrors header `signature` | keep current | not-misnomer |
| `Function::Function::rt` (param) | unsure | low | terse two-letter abbrev | `returnType` (low), keep current (low) | — |
| `Function::Function::parentScope` (param) | no | medium | matches use-site role | keep current | not-misnomer |
| **Resources::Function::addBody** | | | | | |
| `Function::addBody::node` (param) | no | low | name fits AST argument | keep current | — |
| **Resources::Function::sameArgString** | | | | | |
| `Function::sameArgString::sig` (param) | no | medium | mirrors header field | keep current | — |
| `Function::sameArgString::cvar_str` (local) | no | low | descriptive | keep current | — |
| `Function::sameArgString::const_str` (local) | no | low | descriptive | keep current | — |
| `Function::sameArgString::var_str` (local) | no | low | descriptive | keep current | — |
| `Function::sameArgString::norm_sig` (local) | no | low | descriptive | keep current | — |
| `Function::sameArgString::norm_arg` (local) | no | low | descriptive | keep current | — |
| **Resources::Function::isSame** | | | | | |
| `Function::isSame::name` (param) | no | medium | name compared to `this->name` | keep current | not-misnomer |
| `Function::isSame::sig` (param) | no | medium | passed to `sameArgString` | keep current | not-misnomer |
| **Resources::Function::addArgument** | | | | | |
| `Function::addArgument::name` (param) | no | medium | propagated as variable name | keep current | not-misnomer |
| `Function::addArgument::type` (param) | no | medium | dispatched on as VarType | keep current | not-misnomer |
| `Function::addArgument::temp` (local) | unsure | low | generic but bounded scope | `arg`, keep current | — |
| **Resources::Function::addArguments(shared_ptr<Expression>)** | | | | | |
| `Function::addArguments::expr` (param) | no | medium | matches type and use | keep current | not-misnomer |
| `Function::addArguments::p` (local) | unsure | low | bare letter for raw ptr | `exprPtr`, keep current | — |
| **Resources::Function::addArguments(SeqCAstNode const&)** | | | | | |
| `Function::addArguments(SeqCAstNode)::node` (param) | no | medium | matches base-class type | keep current | not-misnomer |
| `Function::addArguments(SeqCAstNode)::paramList` (local) | no | medium | dynamic_cast result | keep current | — |
| `Function::addArguments(SeqCAstNode)::ps` (local) | unsure | low | non-descript abbrev | `paramNodes`, keep current | — |
| `Function::addArguments(SeqCAstNode)::paramNode` (local) | no | low | descriptive | keep current | — |
| `Function::addArguments(SeqCAstNode)::paramName` (local) | no | low | descriptive | keep current | — |
| `Function::addArguments(SeqCAstNode)::nodeName` (local) | no | low | descriptive | keep current | — |
| **Resources::Function::resetScope** | | | | | |
| `Function::resetScope::oldScope` (local) | no | medium | descriptive role | keep current | — |
| `Function::resetScope::oldReturnType` (local) | no | low | descriptive | keep current | — |
| `Function::resetScope::oldReturnValue` (local) | no | low | descriptive | keep current | — |
| `Function::resetScope::oldRegInt` (local) | no | low | descriptive | keep current | — |
| `Function::resetScope::parent` (local) | no | low | descriptive | keep current | — |
| `Function::resetScope::fullName` (local) | unsure | low | concat of name+signature | `concatenatedName`, keep current | — |
| **Resources::functionExistsInScope** | | | | | |
| `Resources::functionExistsInScope::name` (param) | no | medium | name compared to `Function::name` | keep current | not-misnomer |
| `Resources::functionExistsInScope::sig` (param) | no | medium | passed to `sameArgString` | keep current | not-misnomer |
| `Resources::functionExistsInScope::fp` (local) | no | low | range-for binding | keep current | — |
| **Resources::functionExists** | | | | | |
| `Resources::functionExists::name` (param) | no | medium | propagated to lookup | keep current | not-misnomer |
| `Resources::functionExists::sig` (param) | no | medium | gates phase A vs B | keep current | not-misnomer |
| `Resources::functionExists::fn` (local) | no | low | shared_ptr<Function> | keep current | — |
| `Resources::functionExists::fp` (local) | no | low | range-for binding | keep current | — |
| `Resources::functionExists::parent` (local) | no | low | locked weak_ptr | keep current | — |
| **Resources::getFunction** | | | | | |
| `Resources::getFunction::name` (param) | no | medium | matched against `Function::name` | keep current | not-misnomer |
| `Resources::getFunction::sig` (param) | no | medium | passed to `sameArgString` | keep current | not-misnomer |
| `Resources::getFunction::fp` (local) | no | low | range-for binding | keep current | — |
| `Resources::getFunction::parent` (local) | no | low | locked weak_ptr | keep current | — |
| **Resources::getPossibleFunctions** | | | | | |
| `Resources::getPossibleFunctions::name` (param) | no | medium | matched against `Function::name` | keep current | not-misnomer |
| `Resources::getPossibleFunctions::result` (local) | no | low | output vector | keep current | — |
| `Resources::getPossibleFunctions::fp` (local) | no | low | range-for binding | keep current | — |
| `Resources::getPossibleFunctions::parent` (local) | no | low | locked weak_ptr | keep current | — |
| `Resources::getPossibleFunctions::parentResults` (local) | no | low | descriptive | keep current | — |
| **Resources::addFunction** | | | | | |
| `Resources::addFunction::name` (param) | no | medium | propagated | keep current | not-misnomer |
| `Resources::addFunction::sig` (param) | no | medium | propagated | keep current | not-misnomer |
| `Resources::addFunction::rt` (param) | unsure | low | terse two-letter abbrev | `returnType` (low), keep current (low) | coordinated-rename |
| `Resources::addFunction::self` (local) | no | low | shared_from_this result | keep current | — |
| `Resources::addFunction::func` (local) | no | low | new Function | keep current | — |
| **Resources::getRegister** | | | | | |
| `Resources::getRegister::name` (param) | no | medium | variable name | keep current | not-misnomer |
| `Resources::getRegister::var` (local) | no | low | Variable* result | keep current | — |
| `Resources::getRegister::regNum` (local) | no | medium | post-incremented TLS counter | keep current | — |
| **StaticResources::StaticResources** | | | | | |
| `StaticResources::StaticResources::logger` (param) | no | high | logger callback | keep current | not-misnomer |
| `StaticResources::StaticResources::target` (local) | unsure | low | placement-new pointer | `fnTarget`, keep current | — |
| **StaticResources::~StaticResources** | | | | | |
| `StaticResources::~StaticResources::target` (local) | unsure | low | dtor pointer | `fnTarget`, keep current | — |
| **StaticResources::getVariable** | | | | | |
| `StaticResources::getVariable::name` (param) | no | medium | variable name | keep current | not-misnomer |
| `StaticResources::getVariable::var` (local) | no | low | Variable* result | keep current | — |
| `StaticResources::getVariable::fn` (local) | no | low | logger function ptr | keep current | — |
| `StaticResources::getVariable::msg` (local) | no | low | formatted log message | keep current | — |
| **StaticResources::init** | | | | | |
| `StaticResources::init::config` (param) | no | high | matches header signature | keep current | not-misnomer |
| `StaticResources::init::deviceConstants` (param) | no | high | matches type DeviceConstants | keep current | not-misnomer |
| `StaticResources::init::deviceType` (local) | no | low | descriptive | keep current | — |
| `StaticResources::init::supports2GHz` (local) | no | low | descriptive | keep current | — |
| `StaticResources::init::n` (local) | yes | low | bare letter, role is port count | `numOutputPorts`, `portCount` | — |
| `StaticResources::init::base` (local) | unsure | low | shift result, used as code base | `baseCode`, keep current | — |
| `StaticResources::init::sampleRate` (local) | no | medium | descriptive | keep current | — |
| `StaticResources::init::emitSampleRate` (local) | no | low | descriptive | keep current | — |
| **`init` named constant** | | | | | |
| `kRate2GHzDeviceMask` (file-local) | no | medium | bitmap of supported devices | keep current | — |
| **GlobalResources::GlobalResources** | | | | | |
| `GlobalResources::GlobalResources::parent` (param) | unsure | low | stored in `parent_` | keep current | cross-batch-arbitration |
| **GlobalResources statics (TLS)** | | | | | |
| `GlobalResources::regNumber` | no | high | in symbol table | keep current | not-misnomer |
| `GlobalResources::labelIndex` | no | high | in symbol table | keep current | not-misnomer |
| `GlobalResources::random` | no | high | in symbol table | keep current | not-misnomer |
| `GlobalResources::GlobalResources::mult` (local) | no | medium | MT19937-64 multiplier | keep current | — |
| `GlobalResources::GlobalResources::prev` (local) | no | low | last state word | keep current | — |
| **`extern` redeclarations in static_resources.cpp** | | | | | |
| `constAwgIntegrationTrigger` (extern decl) | no | high | matches symbol-table name | keep current | not-misnomer |
| `zsyncDataPqscRegister` (extern decl) | no | high | matches symbol-table name | keep current | not-misnomer |
| `zsyncDataPqscDecoder` (extern decl) | no | high | matches symbol-table name | keep current | not-misnomer |

## 3. Detailed findings

### Function::Function::rt  [unsure / low / —]

Evidence:
- `resources_function.cpp:81` `VarType rt,`
- `resources_function.cpp:87` `      returnType(rt),` — copied verbatim into the field `returnType`
- `resources_function.cpp:106` `    scope->returnType_ = returnType;`
- `resources.hpp:308` declares the param identically: `VarType rt,`

Interpretation:
- The parameter is a `VarType` value used exclusively to initialise the
  function's `returnType` field, which is then propagated into the
  child scope's `returnType_`. The literal-mathematical contraction
  `rt` is a 2-letter abbreviation; everywhere else in the codebase
  this concept is spelled `returnType` / `returnType_`.

Judgement:
- Borderline — short but unambiguous given the surrounding mem-init.
  Worth flagging because the same field's full name is used everywhere
  else; arbitration belongs with sub-batch 19a since the header is the
  canonical declaration site.

Proposals:
- `returnType` (low) — aligns with the field and the matching parameter
  on `Resources::addFunction`.
- keep current (low) — the binary's own naming convention (header was
  reconstructed as `rt` too).

Locations consulted:
- declared: include/zhinst/runtime/resources.hpp:308 (definition site), src/runtime/resources_function.cpp:81

### Resources::addFunction::rt  [unsure / low / coordinated-rename]

Evidence:
- `resources_function.cpp:773` `                       VarType rt) {`
- `resources_function.cpp:780–781` `auto func = std::make_shared<Function>(name, sig, rt, std::weak_ptr<Resources>(self));`
- `resources.hpp:431` declares the parameter as `VarType rt`

Interpretation:
- `rt` is forwarded directly to the `Function` constructor whose own
  parameter is also named `rt`. The two are tied; if one is renamed
  the other should follow.

Judgement:
- Same borderline as `Function::Function::rt`. Worth listing as a
  coordinated rename if synthesis chooses to expand `rt → returnType`.

Proposals:
- `returnType` (low) — paired with `Function::Function::rt`.
- keep current (low).

Cross-reference:
- Coordinated with `Function::Function::rt` (same file) and the header
  declarations in batch 19a.

Locations consulted:
- declared: include/zhinst/runtime/resources.hpp:431, src/runtime/resources_function.cpp:773

### StaticResources::init::n  [yes / low / —]

Evidence:
- `static_resources.cpp:211` `int n = deviceConstants.numOutputPorts;  // byte at offset 0x78`
- `static_resources.cpp:212` `int base = 1 << n;`
- `static_resources.cpp:213–217` `n` is then used as the shift amount
  for the ZSYNC_DATA / QA_DATA `addConst` value calculations.

Interpretation:
- `n` is a single-letter local naming an unambiguously-named source
  field (`numOutputPorts`). The role is "number of output ports"; the
  shift `1 << n` derives the lowest unused channel bit, used as the
  base code for ZSYNC_DATA constants.

Judgement:
- Yes — the role is specific and the source field already carries the
  precise name. Single-letter name is a placeholder.

Proposals:
- `numOutputPorts` (medium) — matches the source field exactly.
- `portCount` (low) — shorter alternative.

Locations consulted:
- defined: src/runtime/static_resources.cpp:211
- used:    src/runtime/static_resources.cpp:212–217

### GlobalResources::GlobalResources::parent  [unsure / low / cross-batch-arbitration]

Evidence:
- `global_resources.cpp:33` `    std::shared_ptr<Resources> const& parent)`
- `global_resources.cpp:38` `    parent_ = parent;` — overwrites the
  base-class `parent_` shared_ptr that was zero-initialised by
  `Resources::Resources("global", weak_ptr<Resources>{})`.
- `resources.hpp:570` declares the parameter identically.
- nm: `Resources::Resources(string const&, weak_ptr<Resources>)` —
  base class takes a `weak_ptr<Resources>`, *not* a `shared_ptr`.

Interpretation:
- `GlobalResources` differs from its base class: the base stores parent
  as `weak_ptr<Resources> parentWeak_` (+0x40), while `GlobalResources`
  stores a strong `shared_ptr<Resources> parent_` (+0x18). The
  parameter name `parent` is correct but does not advertise the
  strong-ownership semantics that distinguish this class from its
  base. The pair `Resources::parent_` (strong, +0x18) and
  `Resources::parentWeak_` (weak, +0x40) are themselves potentially
  misleading and live in batch 19a.

Judgement:
- Borderline — name is accurate but invites confusion with the
  base-class weak parent. Decision should follow whatever is decided
  for the matching field in 19a.

Proposals:
- keep current (medium) — the parameter is genuinely a parent.
- `strongParent` (low) — emphasises the strong-vs-weak distinction.

Cross-reference:
- Counterparts `Resources::parent_` and `Resources::parentWeak_` in
  batch 19a.

Locations consulted:
- declared: include/zhinst/runtime/resources.hpp:570
- defined:  src/runtime/global_resources.cpp:33
- used:     src/runtime/global_resources.cpp:38

### GlobalResources::regNumber, labelIndex, random  [no / high / not-misnomer]

Evidence (symbol table):
```
000000000000004c b zhinst::GlobalResources::labelIndex
0000000000000050 b zhinst::GlobalResources::random
0000000000000048 b zhinst::GlobalResources::regNumber
```
Plus matching `TLS init function` and `TLS wrapper function` symbols
for all three at `0x1f6090`, `0x1f6140`, `0x1f6160`, `0x1f6180`.

Interpretation:
- All three are **`static` data members** with mangled symbols visible
  in the binary symbol table. Per RULES §3 (tier-1 authoritative),
  `static` data member names are excluded from rename.

Judgement:
- No — names are reproductions of the original symbols.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/runtime/resources.hpp:574–579 (header — out of
  scope here, listed for context)
- defined:  src/runtime/global_resources.cpp:11–13

### StaticResources::init::config / deviceConstants  [no / high / not-misnomer]

Evidence:
- `static_resources.cpp:51–52`
  `void StaticResources::init(AWGCompilerConfig const& config,
                              DeviceConstants const& deviceConstants)`
- nm:
  `zhinst::StaticResources::init(zhinst::AWGCompilerConfig const&, zhinst::DeviceConstants const&)`
- Both are read by name in many places: `config.deviceType` (10×),
  `config.deviceSampleRate`, `deviceConstants.deviceType` (2×),
  `deviceConstants.numOutputPorts`, `deviceConstants.samplingRate`.

Interpretation:
- Parameter types come straight from the symbol table; the parameter
  names mirror their type names with the standard de-capitalised
  convention. No use site contradicts the name.

Judgement:
- No — names accurately describe the role and follow the project's
  naming convention.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/runtime/resources.hpp:501–502
- defined:  src/runtime/static_resources.cpp:51–52
- used:     src/runtime/static_resources.cpp:58–253 (~30 reads)

### `kRate2GHzDeviceMask` (named constant)  [no / medium / —]

Evidence:
- `static_resources.cpp:107` `constexpr uint64_t kRate2GHzDeviceMask = 0x100010100ULL;`
- `static_resources.cpp:110` `((kRate2GHzDeviceMask >> deviceType) & 1ULL);`
- Comment at lines 101–104 explains: bits 0/8/16/32 of the 64-bit
  constant correspond to the four supported device-type indices for
  the 2-GHz AWG_RATE family.

Interpretation:
- The constant is a bitmap selecting the device types that get the
  2-GHz family of AWG_RATE constants. Name and value are consistent.

Judgement:
- No — clear name with an explanatory comment and matching usage.

Proposals:
- keep current (medium)

Locations consulted:
- defined: src/runtime/static_resources.cpp:107
- used:    src/runtime/static_resources.cpp:110

### `extern const std::string` redeclarations in static_resources.cpp  [no / high / not-misnomer]

Evidence:
- `static_resources.cpp:21–23`:
  ```cpp
  extern const std::string constAwgIntegrationTrigger;  // BSS @0xb84690
  extern const std::string zsyncDataPqscRegister;       // BSS @0xb846a8
  extern const std::string zsyncDataPqscDecoder;        // BSS @0xb846c0
  ```
- These are *extern declarations* of constants whose canonical
  definition lives in `error_messages.cpp` (batch 08), already audited.
- The header `error_messages.hpp` already declares them; the local
  `extern` is redundant-but-compatible (noted in a comment at
  lines 16–20).

Interpretation:
- The names are identical to those defined in batch 08 — same
  identifiers, same types, same semantics. Renaming here would have to
  be coordinated with batch 08.

Judgement:
- No — names are already canonical in their owning batch.

Proposals:
- keep current (high)

Cross-reference:
- Defining batch 08 (`error_messages`).

Locations consulted:
- redeclared: src/runtime/static_resources.cpp:21–23
- also redeclared: src/runtime/resources_static_global.cpp:24–26
- canonical: src/core/error_messages.cpp (batch 08)

### Resources::Function::Function::name / signature mirroring  [no / medium / not-misnomer]

Evidence:
- `resources_function.cpp:79–86` parameter list `(name, sig, rt, parentScope)`,
  initialiser list `name(name), signature(sig), returnType(rt)`.
- The `sig` parameter binds to the field `signature`; this short form
  is consistent across `sameArgString::sig`, `isSame::sig`,
  `addFunction::sig`, `getFunction::sig`, `functionExists::sig`,
  `functionExistsInScope::sig`, `getPossibleFunctions` (no sig).

Interpretation:
- The `sig`/`signature` split is intentional: `sig` is the parameter
  shorthand, `signature` is the field. Equivalent shorthand pattern
  appears throughout the file. The asymmetry is conventional and not
  misleading.

Judgement:
- No — both names are accurate; the abbreviation is consistent.

Proposals:
- keep current (medium)

Locations consulted:
- defined: src/runtime/resources_function.cpp:79–93
- used:    src/runtime/resources_function.cpp:210, 256–262, 574–581, 618–637,
           666–679, 770–784

### Resources::Function::addArguments(SeqCAstNode)::ps  [unsure / low / —]

Evidence:
- `resources_function.cpp:478` `std::vector<SeqCAstNode const*> ps = paramList->params();`
- `resources_function.cpp:479` `for (SeqCAstNode const* paramNode : ps) {`

Interpretation:
- `ps` is a non-descript two-letter local naming the parameter-node
  vector. The element variable is correctly named `paramNode`; the
  container could match.

Judgement:
- Borderline — short name in a small scope, but inconsistent with the
  element name.

Proposals:
- `paramNodes` (low) — pluralises `paramNode`.
- keep current (low) — local scope is small.

Locations consulted:
- defined: src/runtime/resources_function.cpp:478
- used:    src/runtime/resources_function.cpp:479

### StaticResources::StaticResources::target / ~StaticResources::target  [unsure / low / —]

Evidence:
- `resources_static_global.cpp:80–82`:
  ```cpp
  auto* target = reinterpret_cast<std::function<...>*>(&functionStorage_);
  ::new (target) std::function<...>(logger);
  functionPtr_ = reinterpret_cast<void*>(target);
  ```
- `resources_static_global.cpp:109–111`:
  ```cpp
  auto* target = reinterpret_cast<std::function<...>*>(&functionStorage_);
  target->~function();
  ```

Interpretation:
- `target` is a generic word that does not advertise that this
  pointer is a typed view of the inline `functionStorage_` buffer.
  A name like `fnTarget` or `inlineFn` would be slightly clearer
  but the local scope is short.

Judgement:
- Borderline — the name is not wrong, just generic.

Proposals:
- `fnTarget` (low) — names the target type.
- keep current (low) — local scope.

Locations consulted:
- defined: src/runtime/resources_static_global.cpp:80, 109
- used:    src/runtime/resources_static_global.cpp:81–83, 110–111

## 4. Symbols inspected and judged routinely fine

Method-name / class-name exclusions per RULES §3 (all appear in
`nm --demangle`):

- All `Resources::Function::*` method names (`Function`, `~Function`,
  `getBody`, `addBody`, `sameArgString`, `isSame`, `addArgument`,
  `addArguments` (both overloads), `resetScope`).
- All `Resources::*` method names defined in
  `resources_function.cpp` (`functionExistsInScope`,
  `functionExists`, `getFunction`, `getPossibleFunctions`,
  `addFunction`, `getRegister`).
- All `StaticResources::*` method names (`StaticResources`,
  `~StaticResources`, `getVariable`, `init`).
- All `GlobalResources::*` method names (`GlobalResources`,
  `~GlobalResources`).

Routinely-fine in-scope symbols (parameters/locals whose names match
their use without recordable evidence either way):

- `Function::addBody::node` — passed to `node.clone()`.
- `Function::sameArgString::cvar_str / const_str / var_str / norm_sig / norm_arg` — descriptive.
- `Function::isSame::name / sig` — pass-through to compare and
  `sameArgString`.
- `Function::addArgument::name / type` — pass-through to scope->add*.
- `Function::addArguments(Expression)::expr` — pointer to expression.
- `Function::addArguments(SeqCAstNode)::node` — base-class reference,
  matches type.
- `Function::addArguments(SeqCAstNode)::paramList / paramNode / paramName / nodeName` — descriptive.
- `Function::resetScope::oldScope / oldReturnType / oldReturnValue / oldRegInt / parent` — descriptive `old*` prefix denotes saved state.
- `Resources::functionExistsInScope::name / sig / fp` — pass-through.
- `Resources::functionExists::name / sig / fn / fp / parent` — pass-through.
- `Resources::getFunction::name / sig / fp / parent` — pass-through.
- `Resources::getPossibleFunctions::name / result / fp / parent / parentResults` — descriptive.
- `Resources::addFunction::name / sig / self / func` — pass-through.
- `Resources::getRegister::name / var / regNum` — descriptive.
- `StaticResources::StaticResources::logger` — std::function<void(string)>
  used as a logger callback (matches header).
- `StaticResources::getVariable::name / var / fn / msg` — pass-through.
- `StaticResources::init::deviceType / supports2GHz / sampleRate /
  emitSampleRate / base` — descriptive locals (note: `base` is a
  borderline name but its 3-line scope makes it clear).
- `GlobalResources::GlobalResources::mult / prev` — MT19937-64
  initialisation locals; names match the algorithm's literature.
- `extern const std::string constAwgIntegrationTrigger /
  zsyncDataPqscRegister / zsyncDataPqscDecoder` (in
  `resources_static_global.cpp:24–26` and `static_resources.cpp:21–23`)
  — redeclarations of batch-08 symbols.

## 5. Coverage

**Fully covered** (all four `.cpp` files in scope):

- `reconstructed/src/runtime/resources_function.cpp` — every parameter, every
  local, no in-file constants/macros to consider.
- `reconstructed/src/runtime/resources_static_global.cpp` — every parameter
  and local of the three method definitions; `extern` redeclarations.
- `reconstructed/src/runtime/static_resources.cpp` — every parameter and
  local of `init`; the file-local `kRate2GHzDeviceMask` constant;
  `extern` redeclarations.
- `reconstructed/src/runtime/global_resources.cpp` — ctor parameter, ctor
  locals, the three TLS static data member definitions (excluded as
  tier-1).

**Deferred:** none.

**Not covered (out of scope per RULES §2/§3):**

- All field declarations of `Resources`, `Resources::Function`,
  `StaticResources`, `GlobalResources` (e.g. `name`, `signature`,
  `returnType`, `arguments`, `scope`, `body`, `reserved_`, `weakRef_`,
  `pad_44`, `parent_`, `parentWeak_`, `returnType_`, `returnValue_`,
  `returnReg_`, `variables_`, `functions_`, `usedSampleRate_`,
  `functionStorage_`, `functionPtr_`). Declaration site is
  `resources.hpp` → sub-batch **19a**.
- All method names — present in the binary symbol table (RULES §3).
- All class/struct/enum type names mentioned (`Resources`,
  `StaticResources`, `GlobalResources`, `Resources::Function`,
  `Resources::Variable`, `AWGCompilerConfig`, `DeviceConstants`,
  `SeqCAstNode`, `SeqCParamList`, `SeqCVariable`, `Expression`,
  `Value`, `AsmRegister`, `VarType`, `VarSubType`, `EOperationType`)
  — all appear in `nm --demangle` and/or are owned by other batches.
- Constants `constAwgIntegrationTrigger`, `zsyncDataPqscRegister`,
  `zsyncDataPqscDecoder` — defined in `error_messages.cpp` → batch
  **08**.
- Enum members like `VarType_Var`, `VarType_String`, `VarType_Const`,
  `VarType_Wave`, `VarType_Cvar`, `VarType_Void`, `VarSubType_Default`,
  `VarSubType_FunctionArg`, `EOperationType::ePARAMLIST`, `UHFQA`,
  `UHFLI`, `HDAWG`, `SHFQA`, `SHFSG`, `SHFQC_SG`, `SHFLI`, `VHFLI`,
  `GHFLI`, error codes (`AlreadyDefined`, `UninitializedVar`,
  `DeprecatedConst`, `FormatVarReturn`, `FuncInvalidArgType`) —
  declared in their owning headers (batches 01 / 08 / 22 / 23).
- `Compiler::usedSampleRate_` arbitration is recorded in batch 07;
  `StaticResources::usedSampleRate_` is the counterpart and lives in
  batch 19a (not here).
