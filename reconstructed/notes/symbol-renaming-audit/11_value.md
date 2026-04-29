# Batch 11 — value

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 5 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 2;
> B4 (wontfix / kept-as-is): 1.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/value.hpp`
- `reconstructed/src/value.cpp`

Adjacent (read for cross-reference, not in scope here):
- `reconstructed/include/zhinst/address_impl.hpp` — owns `AddressImpl<T>` and
  its `value` member, header itself out of scope per the assignment.
- `reconstructed/include/zhinst/eval_result_value.hpp` — for the
  cross-batch arbitration on `Value::value_` (see §3).

`nm --demangle _seqc_compiler.so` was consulted for type/method exclusion.
Confirmed in symbol table (excluded from rename per §3, tier-1):
`zhinst::Value`, `zhinst::Value::Value(string)`, `zhinst::Value::~Value`,
`zhinst::Value::toDouble`, `zhinst::Value::toInt`, `zhinst::Value::toBool`,
`zhinst::Value::toString`, `zhinst::Value::operator==`,
`zhinst::Immediate`, all four `Immediate::Immediate` ctors,
`zhinst::Immediate::~Immediate`, `zhinst::ValueException` and its ctor/dtor,
`zhinst::toString(zhinst::Immediate)`,
`zhinst::operator<<(ostream&, zhinst::Immediate)`,
`zhinst::detail::AddressImpl<unsigned int>` (appears in many mangled
parameter types — type name authoritative).

NOT in symbol table (in scope — non-static members, params, locals,
enum members, free-symbol enums, helper enums introduced in
reconstruction): everything called out in §2 below.
`Value::Value()` (default ctor) is not in the binary either; the
report flags its parameter-list naturally has no params, so only
its body locals are inspectable.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Immediate::Storage` (type, nested) | no | medium | well-named union wrapper | keep current (medium) | not-misnomer |
| `Immediate::Storage::address` | no | high | matches `AddressImpl` content | keep current (high) | not-misnomer |
| `Immediate::Storage::integer` | no | medium | matches `int32_t` content | keep current (medium) | — |
| `Immediate::Storage::str` | no | medium | matches `std::string` content | keep current (medium) | — |
| `Immediate::data_` | unsure | low | generic but accurate enough | keep current (medium), `storage_` (low) | — |
| `Immediate::index_` | unsure | low | generic; `kind_`/`tag_` clearer | keep current (medium), `kind_` (low), `tag_` (low) | — |
| `Immediate::Immediate(AddressImpl)::addr` | no | medium | param matches its type | keep current (medium) | — |
| `Immediate::Immediate(uint32_t)::v` | yes | low | terse & ambiguous (addr vs int) | `addrValue` (medium), `address` (low) | — |
| `Immediate::Immediate(int32_t)::v` | unsure | low | terse but unambiguous in context | keep current (medium), `value` (low) | — |
| `Immediate::Immediate(string)::s` | no | low | conventional terse string param | keep current (medium) | — |
| `Immediate::Immediate(Immediate const&)::other` | no | high | conventional copy-ctor name | keep current (high) | not-misnomer |
| `Immediate::Immediate(Immediate&&)::other` | no | high | conventional move-ctor name | keep current (high) | not-misnomer |
| `Immediate::operator=(...)::other` | no | high | conventional assignment name | keep current (high) | not-misnomer |
| `Immediate::holdsUnsigned` (method name) | — | — | symbol-table excluded | — | — |
| `Immediate::operator==::other` | no | high | conventional comparator name | keep current (high) | not-misnomer |
| `toString(Immediate)::imm` | no | medium | matches type abbreviation | keep current (medium) | — |
| `operator<<(ostream&, Immediate)::os` | no | high | universal stream-op convention | keep current (high) | not-misnomer |
| `operator<<(ostream&, Immediate)::imm` | no | medium | matches type abbreviation | keep current (medium) | — |
| `operator<<(ostream&, AddressImpl<uint>)::os` | no | high | universal stream-op convention | keep current (high) | not-misnomer |
| `operator<<(ostream&, AddressImpl<uint>)::addr` | no | high | matches `AddressImpl` semantics | keep current (high) | not-misnomer |
| `ImmediateKind` (enum type) | no | medium | reconstruction helper, descriptive | keep current (medium) | — |
| `ImmediateKind::Address` | no | medium | matches `data_.address` slot | keep current (medium) | — |
| `ImmediateKind::Integer` | no | medium | matches `data_.integer` slot | keep current (medium) | — |
| `ImmediateKind::String` | no | medium | matches `data_.str` slot | keep current (medium) | — |
| `ImmediateKind::Valueless` | no | high | matches `std::variant` term | keep current (high) | — |
| `ValueException::msg_` | no | high | matches `what()` return / RTTI text | keep current (high) | not-misnomer |
| `ValueException::ValueException::msg` | no | high | matches member it stores | keep current (high) | — |
| `ValueType` (enum type) | no | medium | matches member name `type_` | keep current (medium) | — |
| `ValueType::Unspecified` | no | medium | matches throw message text | keep current (medium) | — |
| `ValueType::Int` / `Bool` / `Double` / `String` | no | high | match storage union slots | keep current (high) | — |
| `VariantSlot` (enum type) | yes | medium | unused — defined, never referenced | remove (medium), keep current (low) | — |
| `VariantSlot::Int` / `Bool` / `Double` / `String` | yes | medium | unused enumerators | remove with parent (medium) | — |
| `Value::type_` | no | high | matches `ValueType` content | keep current (high) | not-misnomer |
| `Value::pad_04_` | yes | high | not padding — see §3 | `subType_` placeholder (low), keep current (low) | cross-batch-arbitration |
| `Value::which_` | no | medium | matches boost::variant idiom | keep current (medium) | — |
| `Value::pad_0C_` | unsure | low | accurate "padding" label | keep current (medium) | — |
| `Value::Storage` (nested type) | no | high | well-named union wrapper | keep current (high) | not-misnomer |
| `Value::Storage::i` / `b` / `d` / `str` | no | high | conventional union-slot names | keep current (high) | not-misnomer |
| `Value::storage_` | no | high | conventional name for union member | keep current (high) | not-misnomer |
| `Value::Value(string)::s` | no | medium | conventional terse string param | keep current (medium) | — |
| `Value::Value(double)::d` | no | high | matches union slot name | keep current (high) | — |
| `Value::Value(int32_t)::i` | no | high | matches union slot name | keep current (high) | — |
| `Value::Value(bool)::b` | no | high | matches union slot name | keep current (high) | — |
| `Value::Value(Value const&)::other` and copies | no | high | conventional copy/move name | keep current (high) | — |
| `Value::operator=(...)::other` | no | high | conventional assignment name | keep current (high) | — |
| `Value::operator==::other` | no | high | conventional comparator name | keep current (high) | — |
| `Value::~Value::decoded` (local) | no | medium | matches binary computation | keep current (medium) | — |
| `Value::Value(Value const&)::decoded` (local) | no | medium | matches binary computation | keep current (medium) | — |
| `Value::operator=(...)::curDecoded` / `decoded` (locals) | no | medium | descriptive | keep current (medium) | — |
| `Value::operator==::diff` (local) | no | high | matches `fabs(a-b)` math | keep current (high) | — |
| `Value::toBool::s` (local) | no | medium | conventional terse string alias | keep current (medium) | — |
| `Value::toInt::d` (local) | no | high | matches union slot name | keep current (high) | — |

## 3. Detailed findings

### Immediate::Immediate(uint32_t)::v  [yes / low / —]

Evidence:
- `value.hpp:60` — `Immediate(uint32_t v); // 0x290ac0 — sets index=0 (same as AddressImpl)`
- `value.cpp:40-44`:
  ```cpp
  Immediate::Immediate(uint32_t v)  // 0x290ac0
      : index_(0)  // NOTE: same index as AddressImpl — wraps uint into AddressImpl
  {
      data_.address.value = v;
  }
  ```
- `value.cpp:46-50` — sibling ctor `Immediate(int32_t v)` also uses bare
  `v`, so the two `v` parameters do *different* things (one becomes an
  address, one stays a signed int).

Interpretation:
- The parameter is an unsigned address value being wrapped into the
  `AddressImpl<uint32_t>` slot, not a generic "value". The use of the
  same letter `v` in both ctors hides the semantic distinction at call
  sites where overload resolution picks between them based on type.

Judgement:
- Mildly misleading — `v` doesn't convey that this overload routes to
  the address slot, while the int overload's `v` routes to the integer
  slot.

Proposals:
- `addrValue` (medium)
- `address` (low)
- keep current (low)

Locations consulted:
- declared: include/zhinst/value.hpp:60
- defined:  src/value.cpp:40-44
- used: pervasively at hundreds of `Immediate(intExpr)` call sites in
  `custom_functions_play.cpp`, `prefetch_*.cpp`, `asm_commands.cpp` —
  all positional, no keyword arguments visible.

### Immediate::index_  [unsure / low / —]

Evidence:
- `value.hpp:55` — `uint32_t index_;   // after union — see ImmediateKind enum below`
- `value.hpp:80-86` — companion enum `ImmediateKind { Address, Integer,
  String, Valueless = 0xFFFFFFFF }`.
- `value.cpp:130-132` — `holdsUnsigned() { return index_ == 0; }`
- `value.cpp:69,82,103,121,141,156,173-174,190` — every read is a
  switch/comparison treating it as an `std::variant`-style discriminator.

Interpretation:
- `index_` mirrors the `std::variant::index()` member function name
  exactly. That is in fact the precise vocabulary `std::variant` uses.
  A reader who knows the underlying type was a `std::variant` (called
  out in the file header) will find `index_` instantly recognizable.
- On the other hand, the reconstruction has introduced a typed enum
  `ImmediateKind` for the very same slot, which suggests `kind_` would
  read more naturally for callers who don't know the original was a
  variant.

Judgement:
- Borderline. The name accurately describes the value but is slightly
  generic; renaming to `kind_` would couple it to the helper enum.

Proposals:
- keep current (medium)
- `kind_` (low)
- `tag_` (low)

Locations consulted:
- declared: include/zhinst/value.hpp:55
- used: src/value.cpp:35,41,47,53,58,69-94,97-110,120-124,130,140-148,156-163,172-180,190-196

### Immediate::data_  [unsure / low / —]

Evidence:
- `value.hpp:54` — `Storage data_;     // +0x00`
- `value.cpp` lines 37,43,49,55,72-74,83-85,92,105-107,122,142-145,158-160,176-178,191-193 — every access is `data_.address`, `data_.integer`, `data_.str`.

Interpretation:
- The member is the union storage of a variant. `data_` is the kind of
  generic name §4a flags as suspicious in isolation, but here every
  use site is paired with a typed slot accessor (`.address`, `.integer`,
  `.str`), which preserves clarity. `storage_` (used analogously in
  `Value::storage_`) would be slightly more descriptive and consistent
  with the sister class.

Judgement:
- Not strongly misnamed; cross-class consistency with `Value::storage_`
  is a weak argument.

Proposals:
- keep current (medium)
- `storage_` (low) — matches `Value::storage_`

Locations consulted:
- declared: include/zhinst/value.hpp:54
- used: throughout src/value.cpp (lines above)

### VariantSlot (enum type) and its members  [yes / medium / —]

Evidence:
- `value.hpp:122-129`:
  ```cpp
  // VariantSlot — variant discriminator for Value::which_ (A3)
  // Maps to the union storage slot index, NOT to ValueType.
  enum class VariantSlot : int32_t { Int=0, Bool=1, Double=2, String=3 };
  ```
- Sitewide grep `VariantSlot` returns only its own declaration in
  `value.hpp` plus a mention in `notes/magic_numbers_proposal.md`. No
  source `.cpp` file references the type or any of its members.
- `Value::which_` is consistently written with bare integer literals
  (`which_ = 0`, `which_ = 2`, `which_ = 3`) at every assignment site
  in `resources.cpp`, `waveform_generator.cpp`, `custom_functions.cpp`,
  and `value.cpp` itself (initializers at hpp:190-192 and ctor body at
  cpp:238).

Interpretation:
- The enum was introduced as a documentation/cleanup helper but has
  no callers. It is dead code. Either the in-source magic numbers
  should be replaced with `static_cast<int32_t>(VariantSlot::X)` (the
  intent that motivated the enum) or the enum should be deleted as
  noise.

Judgement:
- The name is fine in isolation but the *symbol* is misleading by
  existence — it implies a documented mapping that is not actually
  used. The misnomer here is that nothing at the use sites references
  it, so its presence misrepresents the level of typing in the code.

Proposals:
- remove `VariantSlot` and its members (medium) — and either replace
  magic-number assignments with the enum or simply leave the comments
- keep current (low) — only acceptable if a follow-up wires the enum
  through the assignment sites
- rename to `WhichSlot` to match `which_` (low)

Locations consulted:
- declared: include/zhinst/value.hpp:122-129
- references: none in src/**.cpp; only notes/magic_numbers_proposal.md:63

### Value::pad_04_  [yes / high / cross-batch-arbitration]

Evidence:
- `value.hpp:168-171`:
  ```cpp
  ValueType type_;       // +0x00
  int32_t   pad_04_{};   // +0x04 — alignment padding (was incorrectly which_)
  int32_t   which_;      // +0x08 — variant discriminator
  int32_t   pad_0C_{};   // +0x0C — alignment padding
  ```
- File-header comment at `value.hpp:150-157` ("CORRECTION 2026-04-23
  (Phase 15a-i)") attributes this slot to "natural alignment padding
  from the compiler putting which_ (int32) on an 8B boundary because
  the storage union requires 8B alignment".
- However: `eval_result_value.hpp:42-46` declares the very analogous
  outer struct as
  ```cpp
  VarType     varType_;      // +0x00
  VarSubType  varSubType_;   // +0x04
  Value       value_;        // +0x08 — embedded Value (0x28 bytes)
  ```
  with `varSubType_` occupying the +0x04 slot, **not** padding. The
  file-header note at `eval_result_value.hpp:18-21` even calls out the
  prior reconstruction error: "Fields renamed from opaque
  field_00/field_08/which_/data_/field_30 to meaningful names" —
  i.e. on `EvalResultValue` the same +4 slot was discovered to carry a
  real `VarSubType`.
- `resources.cpp:589-595` writes `out.value_.type_ = var->value.type_`
  but never touches `pad_04_`, consistent with this being either real
  padding *or* an unused field.
- `resources.cpp` and `custom_functions.cpp` have multiple comments
  referencing "the +0x04 slot" being involved in writes (e.g.
  `resources.cpp:589-590` "overlapping subTypeRaw (4 bytes) +
  value.type_ (4 bytes)").

Interpretation:
- The name `pad_04_` asserts the slot is alignment padding. The
  comparison with `EvalResultValue::varSubType_` and the resources.cpp
  callout to "subTypeRaw (4 bytes)" raise the possibility that the +4
  slot in `Value` itself carries semantic data (a sub-type) too,
  paralleling the outer `EvalResultValue` layout. The current label
  closes off that question by claiming "padding".
- The comment "(was incorrectly which_)" is a self-referential note
  about a prior reconstruction mistake; it does not constitute
  positive evidence that the slot is truly padding — only that it is
  not `which_`.

Judgement:
- The current name overclaims certainty: it names the slot as padding
  without a binary-side observation that the slot is actually unused.
  This is the §4c he-said/she-said pattern: either `Value` has a
  hidden sub-type field at +4 (in which case `pad_04_` is wrong), or
  `EvalResultValue` is the only place that field exists (in which
  case `pad_04_` is right). The decision belongs to the audit of
  `eval_result_value` (batch 12), which has the strongest evidence.

Proposals:
- keep current (low) — if batch 12 confirms `Value` itself has no
  sub-type slot
- `subType_` (low) — if batch 12 finds writes/reads to `Value+4` in
  the binary that aren't accounted for by `EvalResultValue::varSubType_`
- `unused_04_` / `reserved_04_` (low) — neutral framing pending
  arbitration

Cross-reference:
- Counterpart `EvalResultValue::varSubType_` in batch 12
  (`12_eval_result_value.md`).

Locations consulted:
- declared: include/zhinst/value.hpp:168-171
- written:  src/value.cpp:255 (`pad_04_{}`), 275, 285; hpp:190-192
- referenced in comments: src/resources.cpp:589-590, 947-948
- counterpart: include/zhinst/eval_result_value.hpp:42-46

### Value::which_  [no / medium / —]

Evidence:
- `value.hpp:170` — `int32_t which_;      // +0x08 — variant discriminator`
- `value.cpp:496-499` — `~Value() { int decoded = (which_ >> 31) ^ which_; if (decoded >= 3) ...; }`
- File-header comment `value.hpp:11` — "Value: outer ValueType tag +
  boost::variant<int,bool,double,string> (0x28 bytes)".
- `seqc_ast_nodes_evaluate.cpp:2894` comment — "@0x95b8e4, @0x95b8f4)
  keyed on `abs(value.which_)` to copy the ...".

Interpretation:
- `which_` is the canonical Boost.Variant member name for the active
  discriminator (`boost::variant::which()`). Since the file-header
  comment identifies the underlying type as `boost::variant<...>`,
  `which_` is the binary-faithful and idiomatic name. The unusual
  `(w >> 31) ^ w` decoding is a Boost-specific implementation detail
  that further pins this as boost::variant ABI.

Judgement:
- Name is correct; matches the original library's vocabulary.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/value.hpp:170
- used: src/value.cpp:238,277,287,294,297,302-303,312,314,318-319,496;
  src/resources.cpp:198,512,549,595,602,609,616,651,976,1015,1062,1120,1242,1347,1495,1537,1605,1652;
  src/waveform_generator.cpp:167; src/custom_functions.cpp:772,785,787,796

### ValueException::msg_  [no / high / not-misnomer]

Evidence:
- `value.hpp:104` — `std::string msg_;`
- `value.cpp:215-227` — ctor takes `string const& msg`, stores in
  `msg_`, and `what()` returns `msg_.c_str()`.
- `seqc_ast_nodes_evaluate.cpp:3378` comment — "Reads e.msg_ (SSO or
  heap)" — confirms the field is the user-visible exception message.
- `std::exception::what()` semantics: `msg_` matches the standard
  vocabulary (`std::runtime_error::msg_` etc.).

Interpretation:
- The field is the human-readable exception message returned by
  `what()`. `msg_` matches the standard-library convention.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/value.hpp:104
- used: src/value.cpp:216,222,226; referenced in src/seqc_ast_nodes_evaluate.cpp:3378

### Value::Storage and its slot members  [no / high / not-misnomer]

Evidence:
- `value.hpp:172-181`:
  ```cpp
  union Storage {
      int32_t     i;              // which==0, type_==Int
      bool        b;              // which==1, type_==Bool
      double      d;              // which==2, type_==Double
      std::string str;            // which>=3, type_==String
      Storage() : i(0) {}
      ~Storage() {}
  };
  ```
- All write sites use the matching slot for the intended type
  (`storage_.i = length` in `waveform_generator.cpp:168`, `storage_.d = val`
  in `resources.cpp:199`, etc.).
- The comment on each union slot ties the name to the corresponding
  `ValueType` and `which_` value.

Interpretation:
- One-letter slot names (`i`, `b`, `d`) are the conventional
  std::variant-by-hand pattern; `str` is its natural extension. They
  are unambiguous because each slot has a unique type in the union.

Judgement:
- Not a misnomer — slot names match types, and §4a's "bare `data`/`x`"
  warning about generic names doesn't apply because the slot
  identifies its content unambiguously.

Locations consulted:
- declared: include/zhinst/value.hpp:172-182
- used: every `storage_.*` reference cited under `Value::which_`.

### Value::pad_0C_  [unsure / low / —]

Evidence:
- `value.hpp:171` — `int32_t   pad_0C_{};   // +0x0C — alignment padding`
- The +0x0C slot is only mentioned in the layout comment as "(padding)";
  no read/write sites in any source file refer to it.

Interpretation:
- Unlike `pad_04_`, the +0x0C slot has no analogue in any sibling type
  (e.g. `EvalResultValue` has no field there because `Value` itself
  starts at +0x08 there). The "padding" claim is consistent with the
  observation that no code touches it, and it is a natural alignment
  padding before the 8-byte-aligned union storage at +0x10.

Judgement:
- Plausibly correct; recorded `unsure / low` only because the recon
  process for the +0x04 sibling has been re-litigated once before.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/value.hpp:171
- written:  src/value.cpp:257,275,285; hpp:190-192

## 4. Symbols inspected and judged routinely fine

- `Immediate::Storage` (nested union): name describes the role
  (storage for the variant). Same pattern as `Value::Storage`.
- `Immediate::Storage::address` / `integer` / `str`: each slot named
  after its type, conventional std-variant-by-hand pattern.
- `Immediate::Immediate(AddressImpl<uint32_t>)::addr`: standard
  abbreviation matching the type.
- `Immediate::Immediate(int32_t)::v`: terse but unambiguous in a
  single-arg ctor with a non-address integer type.
- `Immediate::Immediate(string)::s`: conventional one-arg-string
  parameter name.
- All copy/move/assign `other` parameters on both `Immediate` and
  `Value`: universal C++ convention.
- `toString(Immediate)::imm`, `operator<<(...)::imm`: established
  abbreviation matching the type name.
- `operator<<(...)::os`: universal `std::ostream` parameter convention.
- `operator<<(ostream&, AddressImpl<uint>)::addr`: matches type
  semantics.
- `ImmediateKind` and members (`Address`, `Integer`, `String`,
  `Valueless`): names track storage slots; `Valueless` matches the
  `std::variant::valueless_by_exception()` vocabulary.
- `ValueException::ValueException::msg`: matches the member it
  initializes.
- `ValueType` and members (`Unspecified`, `Int`, `Bool`, `Double`,
  `String`): `Unspecified` matches the recurring throw message
  "unspecified value type detected in toX conversion" found in
  `value.cpp:343,374,410,442` (binary `.rodata` strings); the four
  numeric/string members match the union slot types.
- `Value::type_`: matches its `ValueType` content; standard convention.
- `Value::storage_`: standard variant-storage member name; consistent
  with `Immediate::data_`'s role on the sister class (see flag on
  `Immediate::data_`).
- `Value::Value(string)::s`, `Value::Value(double)::d`,
  `Value::Value(int32_t)::i`, `Value::Value(bool)::b`: each parameter
  name matches the union slot it writes.
- `Value::~Value::decoded`, `Value::Value(copy)::decoded`,
  `Value::operator=::curDecoded`/`decoded`: name matches the boost
  variant decoding `(w >> 31) ^ w`.
- `Value::operator==::diff`: matches the `fabs(a-b)` epsilon math.
- `Value::toBool::s`: short alias for the storage string within the
  case body.
- `Value::toInt::d`: matches union slot `d` it reads from.

## 5. Coverage

**Fully covered:**
- All non-static data members of `Value`, `Immediate`,
  `Immediate::Storage`, `Value::Storage`, `ValueException`.
- All parameters of every constructor, destructor, conversion
  operator, comparator, free `toString(Immediate)`, and both
  `operator<<` overloads in `value.cpp` (including the `AddressImpl`
  overload defined here).
- All locals introduced in `value.cpp` definitions.
- The two helper enums `ImmediateKind` and `VariantSlot` and every
  enumerator.
- The `ValueType` enum and every enumerator.

**Deferred:**
- None.

**Not covered (out of scope per RULES §2/§3):**
- Type names and method names confirmed in the binary symbol table
  (`Value`, `Immediate`, `ValueException`, `toDouble`, `toInt`,
  `toBool`, `Value::toString`, `Value::operator==`, all `Immediate`
  ctors/dtor, `Immediate::operator==`, `holdsUnsigned`,
  `operator int`, `operator unsigned int`, free `toString(Immediate)`,
  both `operator<<` function names) — excluded by §3 tier-1.
- `detail::AddressImpl<T>` and its `value` member live in
  `address_impl.hpp`, which is not part of this batch's file set.
  Verified `AddressImpl` type name appears in mangled symbols
  (excluded). The `addr.value` member is technically in scope as a
  non-static data member but belongs to a different file's audit;
  noted here as out-of-batch.
