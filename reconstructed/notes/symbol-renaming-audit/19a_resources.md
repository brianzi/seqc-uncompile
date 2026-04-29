# Batch 19a — resources main (header + resources.cpp)

## 1. Files considered

- `reconstructed/include/zhinst/resources.hpp`
- `reconstructed/src/resources.cpp`

Out of scope (sub-batch 19b): `resources_function.cpp`,
`resources_static_global.cpp`, `static_resources.cpp`,
`global_resources.cpp`. Symbols whose declaration lives in
`resources.hpp` but whose only usage is in a 19b file are still in
scope here on the declaration side (params of method declarations
stand on their own; only their bodies live in 19b).

`nm --demangle _seqc_compiler.so` consulted for §3 exclusion.

**Confirmed in symbol table (excluded from rename per §3, tier-1):**

Type names:
- `zhinst::Resources`, `zhinst::StaticResources`, `zhinst::GlobalResources`
- `zhinst::ResourcesException`, `zhinst::VarTypeException`
- `zhinst::Resources::Function` (appears as
  `std::__shared_ptr_emplace<zhinst::Resources::Function, ...>`
  typeinfo name — qualified type in mangling, authoritative)
- `zhinst::VarType`, `zhinst::VarSubType` (appear as parameter types
  in many mangled signatures — type names authoritative)

Free functions:
- `zhinst::str(VarType)` @0x247dd0
- `zhinst::str(VarSubType)` @0x247ee0
- `zhinst::combine(VarType, VarType)` @0x247f50
- `zhinst::combine(VarSubType, VarSubType)` @0x247ea0

Methods of `Resources` (all in `nm` output) — names excluded:
`getVariable`, `setState`, `hasMain`, `createSubScope`, `updateParent`,
`setReturnType`, `getReturnType`, `setReturnValue` (×2), `getReturnValue`,
`setReturnReg`, `getReturnReg`, `getRegisterNumber`,
`variableDependsOnVar`, `variableExists`, `variableExistsInScope`,
`getVariableType`, `getVariableSubType`, `addVar`, `updateVar`,
`checkVar`, `addString` (×2), `updateString`, `readString`,
`addWave` (×2), `updateWave`, `readWave`, `addConst` (×2),
`updateConst`, `readConst`, `constIsSet`, `addCvar` (×2),
`updateCvar`, `readCvar`, `functionExists`, `getFunction`,
`functionExistsInScope`, `getPossibleFunctions`, `addFunction`,
`getRegister`, `newLabel`, `print`, `toString`, `printAll`,
`printScopes`. Resources ctor/dtor; VarTypeException/ResourcesException
ctor/dtor/`what`; StaticResources ctor/dtor and `getVariable`/`init`;
GlobalResources ctor/dtor.

Resources::Function methods (all in `nm` per Batch 5/19b territory but
declared here, names excluded): `Function`, `~Function`, `resetScope`,
`addArgument`, `addArguments` (×2), `addBody`, `getBody`, `isSame`,
`sameArgString`.

Static data members of `GlobalResources` are tier-1 authoritative
(per RULES §3): `regNumber`, `labelIndex`, `random`. **Excluded.**

**Not in symbol table (in scope):** all parameter names, all
non-static data members, all local variables, all enum members
(`VarType_*`, `VarSubType_*`, `Resources::State::*`),
`ResourcesException::msg_`, `VarTypeException::msg_`,
`Resources::*` instance fields,
`Resources::Variable::*` and `Resources::Function::*` field names,
`StaticResources::usedSampleRate_` / `pad_d9_` / `functionStorage_`
/ `functionPtr_` / `pad_108_`, helper `inline bool isConstOrCvar`
(file-local-ish, not in symbol table), `errorReportTarget`
(declared protected; not in `nm`).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `VarType` (enum type) | — | — | name in symbol table | — | — |
| `VarType::VarType_Unset` | no | high | str() returns "notype" | keep current (high) | not-misnomer |
| `VarType::VarType_Void` | no | high | str() returns "void" | keep current (high) | not-misnomer |
| `VarType::VarType_Var` | no | high | str() returns "var" | keep current (high) | not-misnomer |
| `VarType::VarType_String` | no | high | str() returns "string" | keep current (high) | not-misnomer |
| `VarType::VarType_Const` | no | high | str() returns "const" | keep current (high) | not-misnomer |
| `VarType::VarType_Wave` | no | high | str() returns "wave" | keep current (high) | not-misnomer |
| `VarType::VarType_Cvar` | no | high | str() returns "cvar" | keep current (high) | not-misnomer |
| `isConstOrCvar::t` | no | medium | conventional terse param | keep current (medium) | — |
| `VarSubType` (enum type) | — | — | name in symbol table | — | — |
| `VarSubType::VarSubType_Default` | unsure | low | str() prints "none" | keep current (medium), `VarSubType_None` (low) | — |
| `VarSubType::VarSubType_Stub` | yes | medium | str() prints "bool" — see §3 | keep current (medium), `VarSubType_Bool` (low) | — |
| `VarSubType::VarSubType_FunctionArg` | unsure | medium | str() prints "arg" — fits | keep current (high), `VarSubType_Arg` (low) | — |
| `VarSubType::VarSubType_Numeric` | yes | medium | str() prints "vect" — see §3 | keep current (low), `VarSubType_Vect` (medium) | — |
| `VarSubType::VarSubType_String` | unsure | low | str() returns "" — see §3 | keep current (medium) | — |
| `VarSubType_Bool` (legacy alias) | yes | high | retained-for-compat alias | remove (high) | — |
| `VarTypeException::msg_` | no | high | conventional what()-payload name | keep current (high) | not-misnomer |
| `VarTypeException::VarTypeException::msg` | no | high | matches member it stores | keep current (high) | — |
| `combine(VarType,VarType)::lhs` / `::rhs` | no | low | already audited in 04b | keep current (high) | — |
| `combine(VarSubType,VarSubType)::lhs` / `::rhs` | no | low | already audited in 04b | keep current (high) | — |
| `Resources::State` (nested enum type) | no | medium | reconstruction helper, descriptive | keep current (medium) | — |
| `Resources::State::Unset` / `Active` / `Paused` / `Locked` | unsure | low | toString labels not verified | keep current (medium) | — |
| `Resources::Variable` (nested type) | — | — | type name in symbol table | — | — |
| `Resources::Variable::type` | no | high | matches VarType content | keep current (high) | not-misnomer |
| `Resources::Variable::subTypeRaw` | unsure | medium | "Raw" suffix unmotivated | keep current (medium), `subType` (medium) | — |
| `Resources::Variable::value` | no | high | embedded Value object | keep current (high) | not-misnomer |
| `Resources::Variable::reg` | no | high | AsmRegister assignment | keep current (high) | not-misnomer |
| `Resources::Variable::name` | no | high | std::string variable name | keep current (high) | not-misnomer |
| `Resources::Variable::flags` | unsure | medium | only 2 bits used | keep current (medium), `state_` (low) | — |
| `Resources::Variable::pad_52` | no | high | true 6-byte tail padding | keep current (high) | not-misnomer |
| `Resources::VarFlag_Written` | no | high | matches "set/written" semantics | keep current (high) | not-misnomer |
| `Resources::VarFlag_Frozen` | no | high | matches +0x51 frozen-arg semantics | keep current (high) | not-misnomer |
| `Resources::Variable::~Variable` (param-less) | — | — | no params | — | — |
| `Resources::Function::reserved_` | yes | medium | always zero — see §3 | `vptrPad_` (low), `pad_00_` (medium), keep current (low) | — |
| `Resources::Function::weakRef_` | unsure | low | non-standard 8B layout | keep current (medium) | — |
| `Resources::Function::name` | no | high | std::string function name | keep current (high) | not-misnomer |
| `Resources::Function::signature` | no | high | argument-string signature | keep current (high) | not-misnomer |
| `Resources::Function::returnType` | no | high | matches VarType content | keep current (high) | not-misnomer |
| `Resources::Function::pad_44` | no | high | enum-tail 4-byte padding | keep current (high) | not-misnomer |
| `Resources::Function::arguments` | no | high | vector of Variable params | keep current (high) | not-misnomer |
| `Resources::Function::scope` | no | high | child Resources scope | keep current (high) | not-misnomer |
| `Resources::Function::body` | no | high | unique_ptr to AST body | keep current (high) | not-misnomer |
| `Function::Function::name` / `sig` / `rt` / `parentScope` | no | medium | match member targets | keep current (medium) | — |
| `Function::addArgument::name` / `type` | no | high | obvious param semantics | keep current (high) | — |
| `Function::addArguments::expr` / `node` | no | medium | match overload subjects | keep current (medium) | — |
| `Function::addBody::node` | no | medium | matches `body = node.clone()` | keep current (medium) | — |
| `Function::isSame::name` / `sig` | no | high | mirror-of-stored-fields names | keep current (high) | — |
| `Function::sameArgString::sig` | no | high | argument signature string | keep current (high) | — |
| `Resources::Resources::name` / `parent` | no | high | direct field-init pair | keep current (high) | — |
| `Resources::parent_` | yes | high | NOT direct parent — see §3 | `grandparent_` (medium), keep current (low) | cross-batch-arbitration |
| `Resources::name_` | no | high | scope name (printed) | keep current (high) | not-misnomer |
| `Resources::parentWeak_` | unsure | medium | true direct parent — see §3 | keep current (medium), `parent_` (low) | cross-batch-arbitration |
| `Resources::state_` | no | medium | matches State enum content | keep current (medium) | — |
| `Resources::returnType_` | no | high | function-return VarType | keep current (high) | not-misnomer |
| `Resources::returnValue_` | no | high | function-return Value | keep current (high) | not-misnomer |
| `Resources::returnReg_` | no | high | function-return AsmRegister | keep current (high) | not-misnomer |
| `Resources::scopeBoundaryFlags_` | unsure | medium | only low byte used as bool | keep current (medium), `atScopeBoundary_` (medium) | — |
| `Resources::pad_8A_` | no | high | true 6-byte padding | keep current (high) | not-misnomer |
| `Resources::variables_` | no | high | vector<Variable> content | keep current (high) | not-misnomer |
| `Resources::functions_` | no | high | vector<shared_ptr<Function>> | keep current (high) | not-misnomer |
| `Resources::children_` | no | high | vector of child scopes | keep current (high) | not-misnomer |
| `Resources::setState::s` | no | medium | terse but unambiguous | keep current (medium) | — |
| `Resources::createSubScope::name` | no | high | child scope short name | keep current (high) | — |
| `Resources::updateParent::p` / `Resources::updateParent::p` | no | medium | terse but unambiguous | keep current (medium) | — |
| `Resources::setReturnType::type` | no | high | VarType being stored | keep current (high) | — |
| `Resources::setReturnValue(double)::val` | no | high | value being stored | keep current (high) | — |
| `Resources::setReturnValue(Value)::val` | no | high | value being stored | keep current (high) | — |
| `Resources::setReturnReg::reg` | no | high | register-number being stored | keep current (high) | — |
| `Resources::atScopeBoundary` (method) — params none | — | — | — | — | — |
| `Resources::setAtScopeBoundary::v` | no | medium | terse boolean param | keep current (medium) | — |
| `Resources::variableDependsOnVar::name` | no | high | variable name to query | keep current (high) | — |
| `Resources::variableExists*::name` | no | high | variable name to query | keep current (high) | — |
| `Resources::getVariableType::name` | no | high | variable name to query | keep current (high) | — |
| `Resources::getVariableSubType::name` | no | high | variable name to query | keep current (high) | — |
| `Resources::addVar::name` / `st` | no | medium | matches usage; `st` terse but consistent | keep current (medium) | — |
| `Resources::updateVar::name` | no | high | name to update | keep current (high) | — |
| `Resources::checkVar::name` | no | high | name to check | keep current (high) | — |
| `Resources::add{String,Wave}(name,val)::name` / `val` | no | high | matches semantics | keep current (high) | — |
| `Resources::add{String,Wave}(name,st)::st` | no | medium | see `addVar::st` | keep current (medium) | — |
| `Resources::update{String,Wave}::name` / `val` / `st` | no | high | matches semantics | keep current (high) | — |
| `Resources::read{String,Wave,Const,Cvar}::name` / `dir` | no | high | matches usage | keep current (high) | — |
| `Resources::add{Const,Cvar}(name,val,st)::name` / `val` / `st` | no | high | matches semantics | keep current (high) | — |
| `Resources::add{Const,Cvar}(name,st)::name` / `st` | no | medium | matches usage | keep current (medium) | — |
| `Resources::updateConst::name` / `val` / `st` / `force` | no | high | obvious semantics | keep current (high) | — |
| `Resources::updateCvar::name` / `val` / `st` | no | high | obvious semantics | keep current (high) | — |
| `Resources::constIsSet::name` | no | high | name to query | keep current (high) | — |
| `Resources::functionExists*::name` / `sig` | no | high | match member field pair | keep current (high) | — |
| `Resources::getFunction::name` / `sig` | no | high | as above | keep current (high) | — |
| `Resources::getPossibleFunctions::name` | no | high | name to query | keep current (high) | — |
| `Resources::addFunction::name` / `sig` / `rt` | no | high | match Function ctor pair | keep current (high) | — |
| `Resources::getRegister::name` | no | high | name to query | keep current (high) | — |
| `Resources::newLabel::name` (decl) / `base` (defn) | yes | medium | inconsistency between H/CPP | unify on `base` (medium) | coordinated-rename |
| `getVariable::name` (override too) | no | high | name to look up | keep current (high) | — |
| `Resources::getVariable::result` (local in §3 below) | unsure | low | terse but accurate | keep current (medium) | — |
| `Resources::getVariable::var` (loop var, local) | no | high | conventional loop name | keep current (high) | — |
| `setReturnValue(double)::v` (local) | no | medium | terse but obvious | keep current (medium) | — |
| `Resources::createSubScope::self` (local) | no | high | shared_ptr to *this | keep current (high) | — |
| `Resources::createSubScope::child` (local) | no | high | new child Resources | keep current (high) | — |
| `Resources::createSubScope::fullName` (local) | no | high | concatenated scope name | keep current (high) | — |
| `getReturnType::p` (local) and other `auto p =` lock locals | no | high | conventional weak_ptr lock idiom | keep current (high) | — |
| `addX/updateX/checkX/readX/constIsSet::var` (local) | no | high | conventional pointer-to-Variable name | keep current (high) | — |
| `addX/updateX::v` (local Variable temp) | no | medium | terse, but conventional | keep current (medium) | — |
| `add*::var` (loop var in dup-check) | no | high | conventional loop name | keep current (high) | — |
| `addConst/addCvar/updateConst/updateCvar::val` (param) | no | high | matches stored numeric value | keep current (high) | — |
| `readConst/readCvar::w` and `::absW` (locals) | no | medium | match boost::variant `which_` term | keep current (medium) | — |
| `readConst/readCvar::out` (local) | no | high | sret return value | keep current (high) | — |
| `readConst/readCvar::src` / `dst` (locals) | no | high | bytewise copy buffers | keep current (high) | — |
| `readString/readWave::tmp` (local) | unsure | low | could be `valueStr` | keep current (medium) | — |
| `readString/readWave::embedded` (local) | no | medium | descriptive Value-pointer | keep current (medium) | — |
| `update{String,Wave,Const,Cvar}::frozen` (local) | no | high | matches +0x51 semantics | keep current (high) | — |
| `updateString/updateWave/updateConst/updateCvar::v` (local) | no | medium | terse pointer-to-Variable | keep current (medium) | — |
| `getRegisterNumber::n` (local) | no | high | conventional counter name | keep current (high) | — |
| `newLabel::idx` / `os` (locals) | no | high | conventional names | keep current (high) | — |
| `str(VarType)::vt` | no | high | conventional VarType abbrev | keep current (high) | — |
| `str(VarSubType)::vst` | no | high | conventional VarSubType abbrev | keep current (high) | — |
| `StaticResources::usedSampleRate_` | unsure | low | mirror of Compiler counterpart | keep current (medium), `usedDeviceSampleRate_` (low) | cross-batch-arbitration |
| `StaticResources::pad_d9_` | no | high | true 7-byte padding | keep current (high) | not-misnomer |
| `StaticResources::functionStorage_` | yes | medium | misleading "storage" — it's the std::function's inline buffer | `errorLoggerStorage_` (medium), `loggerInline_` (low) | — |
| `StaticResources::functionPtr_` | yes | medium | misleading — is the std::function's vtable ptr | `errorLoggerVtbl_` (medium), `loggerCallable_` (low) | — |
| `StaticResources::pad_108_` | unsure | low | likely real padding | keep current (medium) | — |
| `StaticResources::StaticResources::logger` | no | high | matches std::function payload | keep current (high) | — |
| `StaticResources::init::config` / `deviceConstants` | no | high | match types | keep current (high) | — |
| `StaticResources::errorReportTarget` (method, no params) | unsure | medium | "errorReport" not yet verified | keep current (medium) | — |
| `StaticResources::getVariable::name` | no | high | name to look up | keep current (high) | — |
| `GlobalResources::GlobalResources::parent` | no | high | parent scope arg | keep current (high) | — |
| `ResourcesException::msg_` | no | high | matches `what()` return | keep current (high) | not-misnomer |
| `ResourcesException::ResourcesException::msg` | no | high | matches member it stores | keep current (high) | — |

## 3. Detailed findings

### `VarSubType::VarSubType_Stub`  [yes / medium / —]

Evidence:
- `resources.hpp:101-109` enum declares `VarSubType_Stub = 1`, with a
  legacy alias `constexpr VarSubType VarSubType_Bool = VarSubType_Stub;`
- `resources.cpp:451` `case VarSubType_Stub: return "bool";`
- `resources.hpp:90-94` comment: "the previous `VarSubType_Bool=1` label
  was misleading — value 1 is the generic 'stub' subtype written by
  addVar and the bare-stub overloads, not specifically a boolean tag."

Interpretation:
- The binary's `str(VarSubType)` jump-table at @0x247ee0 maps slot 1 →
  the literal "bool" string. The reconstruction renamed the enumerator
  from `VarSubType_Bool` to `VarSubType_Stub` because the *usage* in
  add* methods clearly is "uninitialised / stub", not a boolean tag.
- The serializer string ("bool") is §4d tier-2 evidence FOR the old
  name `_Bool`; the usage analysis is contradictory soft evidence.

Judgement:
- Rename was based on real usage analysis but the binary's own
  serializer string is "bool" — the audit should at least record this
  tension rather than silently keep `_Stub`.

Proposals:
- keep current `VarSubType_Stub` (medium) — matches usage semantics
- `VarSubType_Bool` (low) — matches binary's `str()` string; contradicts usage

### `VarSubType::VarSubType_Numeric`  [yes / medium / —]

Evidence:
- `resources.cpp:453` `case VarSubType_Numeric: return "vect";`
- `resources.hpp:101-107` enum-comment: "3 = numeric-with-value
  (full addConst, full addCvar)"

Interpretation:
- Binary serializer string is "vect" (vector). Reconstruction renamed
  to `_Numeric` based on the usage pattern (addConst / addCvar with
  a double value).

Judgement:
- Same tension as `_Stub`: name fits usage but contradicts the
  binary's own string.

Proposals:
- keep current `VarSubType_Numeric` (low)
- `VarSubType_Vect` (medium) — faithful to binary string

### `VarSubType::VarSubType_String`  [unsure / low / —]

Evidence:
- `resources.cpp:454` `default: return {};` — slot 4 (String) returns
  the empty string in the binary's str() jump table.
- `resources.hpp:101-107` comment: "4 = string-bound (full addString,
  full addWave)"

Interpretation:
- Binary has no string for slot 4, so no positive serializer evidence
  either way. Usage analysis (addString/addWave write the caller's
  string into the variant slot) supports `_String`.

Judgement:
- Name fits usage; absence of binary string makes confidence low but
  no rename indicated.

Proposals:
- keep current (medium)

### `VarSubType_Bool` (legacy alias at resources.hpp:109)  [yes / high / —]

Evidence:
- `resources.hpp:108-109`
  `// Legacy alias retained for source compatibility during the cascading fix.
  constexpr VarSubType VarSubType_Bool = VarSubType_Stub;`

Interpretation:
- Self-described as a transient compat alias.

Judgement:
- A migration-only artefact; should be removed once consumers migrate.

Proposals:
- remove (high) — an audit-followup item, not a rename per se

### `Resources::Variable::subTypeRaw`  [unsure / medium / —]

Evidence:
- `resources.hpp:246-249` member `VarSubType subTypeRaw;` with comment
  "the *original* VarSubType arg passed to addX — e.g. addCvar and
  addConst stash the caller's `st` here verbatim. Read by
  getVariableSubType @0x1e4580."
- `resources.cpp:826-834` `getVariableSubType` returns `var->subTypeRaw`.
- `resources.cpp:510, 1060, 1118, ...` all writes are `v.subTypeRaw = st;`
  where `st` is the caller-supplied `VarSubType` parameter.

Interpretation:
- The "Raw" suffix was added during reconstruction to disambiguate from
  a previously-believed *secondary* tag at +0x08. After the Phase
  20e-ii Batch 5a cleanup (resources.hpp:204-262) the +0x08 slot is
  now known to be the embedded Value's `type_`, so there is no longer
  a competing "cooked" subType — the suffix is unmotivated.

Judgement:
- Not a misnomer in a misleading sense, but the "Raw" suffix has
  outlived its disambiguation purpose.

Proposals:
- keep current `subTypeRaw` (medium)
- `subType` (medium) — drop the now-redundant suffix

### `Resources::Variable::flags`  [unsure / medium / —]

Evidence:
- `resources.hpp:259, 268-269` field is `int16_t flags` with two
  named bits: `VarFlag_Written = 0x01` (low byte), `VarFlag_Frozen =
  0x100` (high byte). No other bits are read or written anywhere in
  this file.
- All update sites use `(v->flags & 0xFF00) | VarFlag_Written` —
  treating it as two mutually-exclusive 1-bit zones.

Interpretation:
- Despite the type being `int16_t`, only 2 bits are ever live.
  `flags` is technically accurate but oversells the field.

Judgement:
- Borderline — `flags` is conventional and not actively misleading.

Proposals:
- keep current (medium)
- `state_` (low) — would conflict with `Resources::state_`, not great

### `Resources::Function::reserved_`  [yes / medium / —]

Evidence:
- `resources.hpp:288` `void* reserved_; // +0x00`
- `resources.hpp:276-285` layout comment: "+0x00 8 void*  reserved
  (zeroed in ctor)"

Interpretation:
- Always zeroed by the ctor and never read. Not "reserved" in the
  hardware-register sense; just an unexplained 8-byte slot at the
  head of the struct. Could be a `__shared_ptr_emplace`-related
  pad or vptr-equivalent slot from a base class that was elided in
  reconstruction.

Judgement:
- "reserved_" implies intent that the binary doesn't demonstrate;
  a `pad_*` label would be more faithful to the reconstruction's
  actual evidence.

Proposals:
- `pad_00_` (medium)
- keep current (low)
- `vptrPad_` (low) — speculative

### `Resources::parent_` and `Resources::parentWeak_`  [yes / high / cross-batch-arbitration]

Evidence:
- `resources.hpp:454-456` two adjacent fields:
  `std::shared_ptr<Resources> parent_;     // +0x18`
  `std::weak_ptr<Resources>   parentWeak_; // +0x40`
- `resources.cpp:97-103` (ctor):
  `if (auto p = parentWeak_.lock()) { parent_ = p->parent_; }`
- `resources.cpp:135-139` ctor comment: "parentWeak_ is the direct
  parent link (weak). Binary @0x1e34f1: locks parentWeak_, then copies
  parent->parent_ into this->parent_ (grandparent strong ref)."
- `resources.cpp:308-328` `getVariable` walks via `parentWeak_.lock()`
  FIRST and falls back to `parent_` only if the weak link is dead.
- `resources.cpp:735-740` `variableExists` same precedence.
- `resources.cpp:181-185, 213-218, 230-237, 248-253, 264-269, 901-906`
  every accessor that walks "the parent chain" uses `parentWeak_` as
  the canonical link.
- `resources.cpp:1957-1960` `updateParent(shared_ptr p) { parentWeak_ = p; }`
  — the public setter mutates ONLY `parentWeak_`, leaving `parent_`
  as the stale grandparent reference.

Interpretation:
- The strongly-typed field named `parent_` actually stores the
  *grandparent* (parent->parent_, copied at construction time and
  never updated by `updateParent`). The weakly-typed
  `parentWeak_` is the actual direct parent. Names are inverted
  with respect to graph topology.

Judgement:
- Both names are misleading in opposite directions. This is a clean
  he-said/she-said §4c case: the strong-vs-weak distinction is one
  axis, the parent-vs-grandparent distinction is another, and the
  current naming conflates them.

Proposals:
- `parent_` → `grandparent_` (medium)
- `parentWeak_` → `parent_` (low — would clash with the rename above unless coordinated)
- keep both (low) with stronger comments
- coordinated rename: `grandparent_` (strong) + `parent_` (weak), with
  the existing `parent_` slot first renamed to `grandparent_` to free
  the name (medium overall as a single coordinated change)

Cross-reference:
- These two fields are only consulted in this batch's files; the
  arbitration is internal but tagged `cross-batch-arbitration` because
  any rename would ripple into 19b (resources_function.cpp uses the
  field via `Function::scope` initialisation, etc.).

### `Resources::scopeBoundaryFlags_`  [unsure / medium / —]

Evidence:
- `resources.hpp:175-176, 461` field `int16_t scopeBoundaryFlags_; // +0x88`
- `resources.hpp:386-389` accessors:
  `bool atScopeBoundary() const { return (scopeBoundaryFlags_ & 0xFF) != 0; }`
  `void setAtScopeBoundary(bool v) { scopeBoundaryFlags_ = v ? 1 : 0; }`
- `resources.cpp:212, 230, 304, 314, 322` all reads compare the low byte
  to 0 only — single boolean bit semantics in practice.

Interpretation:
- The 16-bit type and "Flags" suffix imply multiple flag bits, but only
  one bit is ever set or queried (the public API is a `bool`).

Judgement:
- "Flags" plural is not faithful to current usage. Could reasonably be
  renamed to a single-purpose `atScopeBoundary_` to match the accessor.

Proposals:
- keep current (medium) — the 16-bit width hints at a future second flag
- `atScopeBoundary_` (medium) — matches accessor

### `Resources::newLabel` parameter inconsistency  [yes / medium / coordinated-rename]

Evidence:
- `resources.hpp:437` declaration: `static std::string newLabel(std::string const& name);`
- `resources.cpp:680` definition:    `std::string Resources::newLabel(std::string const& base)`

Interpretation:
- The header parameter is `name`, the .cpp parameter is `base`. Both
  refer to the same value — a prefix concatenated with the per-thread
  `labelIndex`. Within the body the variable is treated as a *prefix*
  (defaults to "label" when empty), so `base` is the more accurate
  name.

Judgement:
- A real H/CPP mismatch. Trivial but worth fixing as a coordinated
  rename of the declaration.

Proposals:
- unify on `base` (medium) — header changes to match definition

### `StaticResources::usedSampleRate_`  [unsure / low / cross-batch-arbitration]

Evidence:
- `resources.hpp:488, 513` `bool usedSampleRate_; // +0xD8`
- Cross-batch (07): `Compiler::usedSampleRate_` flagged
  cross-batch-arbitration vs THIS field; see
  `notes/symbol-renaming-audit/07_compiler.md:267-307`.
- The flag is set by SeqC code paths that consume the device sample
  rate (per the comment at resources.hpp:488 and the documented
  Compiler accessor `bool usedDeviceSampleRate() const`).

Interpretation:
- Name is opaque without context. The Compiler counterpart was already
  flagged with the alternative `usedDeviceSampleRate_`, and the
  Compiler accessor that reads a `usedSampleRate_` field is publicly
  named `usedDeviceSampleRate()` — soft evidence (§4a) that the
  intended semantic name carries "Device".

Judgement:
- Same arbitration as Compiler — the two fields should be renamed
  together or both kept.

Proposals:
- keep current (medium) — preserves cross-class symmetry as-is
- `usedDeviceSampleRate_` (low) — matches the public accessor name

Cross-reference:
- Counterpart `Compiler::usedSampleRate_` in batch 07.

### `StaticResources::functionStorage_` and `StaticResources::functionPtr_`  [yes / medium / —]

Evidence:
- `resources.hpp:496` ctor takes `std::function<void(string const&)> const& logger`.
- `resources.hpp:504-518` private fields:
  `char functionStorage_[0x20]; // +0xE0 (inline buffer for std::function)`
  `void* functionPtr_;          // +0x100 (pointer to callable target)`
- `resources.hpp:505-510` (errorReportTarget comment): "Returns a
  callable that forwards to the std::function stored inline at
  (functionStorage_, functionPtr_). The binary at 0x12a256-0x12a26d
  dispatches via `[functionPtr_->vtable + 0x30]` which is the
  standard libc++ `__function::__base::__invoke` entry."

Interpretation:
- `functionStorage_` and `functionPtr_` are halves of an embedded
  `std::function<void(string const&)>` used for *error logging*. The
  generic "function" prefix doesn't reveal the role. The field-pair
  names a libc++ ABI detail, not a domain concept.

Judgement:
- Misleading because "function" is the most generic possible prefix
  in a Resources class that already manages user-defined SeqC
  Functions. A reader scanning the header sees "functionStorage_" and
  reasonably assumes a SeqC-Function-related field.

Proposals:
- `errorLoggerStorage_` / `errorLoggerVtbl_` (medium) — names match
  the documented use of the stored callable
- `loggerInline_` / `loggerCallable_` (low) — terser
- keep current (low)

### `Resources::State::Active` / `Paused` / `Locked`  [unsure / low / —]

Evidence:
- `resources.hpp:191-196` enum with comment "From toString jump table:
  states 1..3 print labels, default throws/skips."
- `resources.cpp:368-372` toString uses
  `const char* stateNames[] = {"Active", "Paused", "Locked"};` —
  these strings are placeholders authored in reconstruction, not lifted
  from the binary (`stateNames` is not read from .rodata in the
  disassembly walk recorded above the function).

Interpretation:
- Tier-2 serializer evidence (§4d) is NOT available — the strings in
  the .cpp are reconstruction guesses, not faithful binary strings.

Judgement:
- Names plausibly fit the documented "first set / forced lock"
  semantics in `setState`, but no positive evidence pins the labels.

Proposals:
- keep current (medium) — lacking binary string evidence
- (no targeted alternative — needs disassembly of toString state branch)

## 4. Symbols inspected and judged routinely fine

- All `*::*::name` parameters that mirror a stored variable / function
  / scope name — conventional.
- All `add*`/`update*`/`read*` parameters `name`, `val`, `st`, `dir`,
  `force` — match the receiving `Variable` slot 1:1.
- `Variable::name`, `::reg`, `::value`, `::type`, `::pad_52` — match
  storage semantics.
- `Function::name`, `::signature`, `::returnType`, `::arguments`,
  `::scope`, `::body`, `::pad_44` — match storage / type semantics.
- `Resources::name_`, `::returnType_`, `::returnValue_`, `::returnReg_`,
  `::variables_`, `::functions_`, `::children_`, `::pad_8A_` — match
  storage / type semantics.
- `Resources::Variable::~Variable()` and `Resources::~Resources()` —
  no parameters.
- `combine(VarType,VarType)` and `combine(VarSubType,VarSubType)`
  parameters — already audited in batch 04b (no rename).
- `str(VarType)::vt`, `str(VarSubType)::vst` — conventional.
- `Resources::createSubScope::self/child/fullName` (locals) — clear
  enough.
- `Resources::*Const/Cvar::w / absW / out / src / dst` (locals) —
  short variable-payload names; standard.
- `update*::frozen` (local) — directly named after `VarFlag_Frozen`.
- `Resources::Resources::name`/`parent`, `Function::Function::name`/`sig`/
  `rt`/`parentScope`, `addArgument::name`/`type`,
  `updateConst::name`/`val`/`st`/`force`, `addFunction::name`/`sig`/`rt`,
  `getRegister::name` — all faithfully mirror the corresponding
  member/argument they bind.
- `VarType_Unset`/`Void`/`Var`/`String`/`Const`/`Wave`/`Cvar` — pinned
  by `str(VarType)` jump-table strings (tier-2 §4d positive evidence).
- `VarTypeException::msg_`, `ResourcesException::msg_` — universal
  what()-payload field name.
- `GlobalResources::GlobalResources::parent` — conventional.
- `StaticResources::StaticResources::logger`,
  `StaticResources::init::config`/`deviceConstants`,
  `StaticResources::getVariable::name` — match types/usage.

## 5. Coverage

**Fully covered:**
- All declared symbols in `reconstructed/include/zhinst/resources.hpp`,
  including the nested `Variable` / `Function` / `State` types and
  static helper constants `VarFlag_Written`/`VarFlag_Frozen`.
- All function/method bodies in `reconstructed/src/resources.cpp`,
  including their local variables.
- Free functions `combine` (×2) and `str` (×2) at the parameter level
  (cross-checked against batch 04b).
- Static thread-local data members of `GlobalResources` (verified
  excluded per `nm`).

**Deferred:**
- The `Resources::State` label strings (`Active`/`Paused`/`Locked`)
  cannot be confirmed without the disassembly of `toString`'s state
  jump table — flagged `unsure` rather than chased here.
- `StaticResources::errorReportTarget` body / use sites — only the
  declaration is in scope; the body lives in batch 19b (the
  resources_static_global / static_resources split).

**Not covered (out of scope per assignment):**
- All method *bodies* whose definitions live in
  `resources_function.cpp` (Function ctor/dtor/addArgument/
  addArguments/addBody/getBody/isSame/sameArgString — the header
  declarations were audited; their local-variable names live in 19b),
  `resources_static_global.cpp`, `static_resources.cpp`,
  `global_resources.cpp` — sub-batch 19b.

**Not covered (out of scope per RULES §3 — symbol-table excluded):**
- All type names (`Resources`, `StaticResources`, `GlobalResources`,
  `Resources::Function`, `Resources::Variable` is not in `nm` directly
  but its parent is, see note below; `VarType`, `VarSubType`,
  `ResourcesException`, `VarTypeException`).
- All free-function names, all method names listed in §1.
- Static data members `GlobalResources::regNumber`,
  `GlobalResources::labelIndex`, `GlobalResources::random`.

Note on `Resources::Variable`: the type name itself does not appear in
`nm` as a standalone mangled symbol (no method on `Variable` is
exported with `Variable` qualifying its name), but the dtor `~Variable`
@0x1e4be0 is listed as `Resources::Variable::~Variable` per the
reconstruction comments and the `__shared_ptr_emplace<Variable>`
mangling found in nm-output. Treated as in-scope only at the **field**
level (per RULES §3, non-static data members are always in scope
regardless of enclosing class status).
