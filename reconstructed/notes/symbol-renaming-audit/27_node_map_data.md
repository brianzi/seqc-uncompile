# Batch 27 — node_map_data

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 9 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 4;
> B4 (wontfix / kept-as-is): 3.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/runtime/node_map_data.hpp`
- `reconstructed/src/runtime/node_map_data.cpp`
- `reconstructed/src/runtime/get_node_map.cpp` (1287 lines — surveyed; almost entirely
  data-table entries calling two anonymous-namespace helpers, no in-scope
  symbols beyond the helpers and their parameters)

Sister/cross-batch context consulted (read-only):
- `reconstructed/src/runtime/custom_functions.cpp` (getNodeAddress, getAccessModes)
- `reconstructed/src/runtime/custom_functions_play.cpp:1430..1610`
  (he-said/she-said evidence around `NodeMapItem::hasFast` ↔ `accessMode`)
- `reconstructed/include/zhinst/runtime/custom_functions.hpp` (AccessMode, NodeMap,
  CustomFunctions::addNodeAccess)
- `reconstructed/notes/struct_layouts.md` (Block-D dispatch structure)
- `reconstructed/notes/writeToNode_block_d_protocol.md`
- `reconstructed/notes/magic_numbers_proposal.md` (origin of NodeTypeIdx)

## 2. Overview

Authoritative-from-binary (not in scope, listed for completeness in §5):

- Type names `NodeMapData`, `VirtAddrNodeMapData`, `DirectAddrNodeMapData`,
  `NodeMapItem`.
- Method names `compareEq`, `hash`, `clone`, `getJson`, `fastAddress`,
  `toJson`, `size`, `operator==`, `~NodeMapData`, `~VirtAddrNodeMapData`,
  `~DirectAddrNodeMapData`, `VirtAddrNodeMapData(VirtAddrNodeMapData const&)`.
  All present in `nm --demangle _seqc_compiler.so`.

In-scope symbol table:

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `NodeTypeIdx` (enum) | unsure | low | name fits, but redundant with NodeMapItem::typeIdx | keep current; `NodeValueEncoding` | — |
| `NodeTypeIdx::IntegerPassthrough` | no | medium | matches dispatch case 0 (no transform) | keep current | not-misnomer |
| `NodeTypeIdx::SinePair` | unsure | low | inferred from sine I/Q usage | keep current; `IqPair` | — |
| `NodeTypeIdx::FloatBits` | no | low | matches case-2 IEEE-754 emit | keep current | — |
| `NodeTypeIdx::RawDoubleLow32` | unsure | low | inferred; case-3 not exhaustively traced | keep current | in-scope (no NodeTypeIdx/RawDoubleLow32 in nm or strings) |
| `NodeTypeIdx::Frequency` | no | medium | matches case-4 toFrequency() codegen | keep current | not-misnomer |
| `NodeTypeIdx::Phase` | no | medium | matches case-5 toPhase() codegen | keep current | not-misnomer |
| `VirtAddrNodeMapData::name_` | no | high | JSON key `"name"` written verbatim | keep current | not-misnomer |
| `VirtAddrNodeMapData::addresses_` | unsure | medium | written under JSON key `"index"` | `index_`; keep current | — |
| `VirtAddrNodeMapData::compareEq::other` | no | low | matches base virtual signature | keep current | — |
| `VirtAddrNodeMapData::compareEq::o` | yes | low | one-letter local | `otherCast` / `rhs` | — |
| `VirtAddrNodeMapData::hash::seed` | no | low | iterative hash-combine accumulator | keep current | — |
| `VirtAddrNodeMapData::hash::v` | no | low | scratch in splitmix | keep current | — |
| `VirtAddrNodeMapData::hash::combined` | no | low | matches usage | keep current | — |
| `VirtAddrNodeMapData::hash::h` | no | low | matches usage (string hash) | keep current | — |
| `VirtAddrNodeMapData::hash::a` | no | low | range-for index, fits | keep current | — |
| `VirtAddrNodeMapData::getJson::obj` | no | medium | parameter is a JSON object out-param | keep current | not-misnomer |
| `VirtAddrNodeMapData::getJson::arr` | no | low | local json::array, fits | keep current | — |
| `kGoldenRatioHash` | no | medium | value 0x9e3779b9 is the well-known constant | keep current | not-misnomer |
| `kMul` | unsure | low | non-descript scratch constant | `kSplitmixMul` | — |
| `DirectAddrNodeMapData::addr_` | no | high | JSON key `"direct_address"` written verbatim | keep current | not-misnomer |
| `DirectAddrNodeMapData::compareEq::other` | no | low | matches base virtual signature | keep current | — |
| `DirectAddrNodeMapData::compareEq::o` | yes | low | one-letter local | `otherCast` / `rhs` | — |
| `DirectAddrNodeMapData::hash::v` | no | low | splitmix scratch | keep current | — |
| `DirectAddrNodeMapData::clone::p` | yes | low | one-letter local | `copy` / `cloned` | — |
| `DirectAddrNodeMapData::getJson::obj` | no | medium | JSON out-param | keep current | not-misnomer |
| `NodeMapItem::data` | no | medium | polymorphic NodeMapData payload | keep current | not-misnomer |
| `NodeMapItem::typeIdx` | unsure | low | "Idx" suffix misleading; this is a kind tag | `typeCode`, `valueKind`; keep current | — |
| `NodeMapItem::fastAddr` | no | medium | matches use as fast-path register address | keep current | not-misnomer |
| `NodeMapItem::hasFast` | yes | medium | conflated with AccessMode arg to addNodeAccess | `accessModeOrHasFast`; keep current; `accessModeRaw` | cross-batch-arbitration |
| `NodeMapItem::pad_11` | no | high | explicit padding field | keep current | not-misnomer |
| `NodeMapItem::operator==::other` | no | low | matches std convention | keep current | — |
| `NodeMapItem::operator==::thisHas` | no | low | local fits | keep current | — |
| `NodeMapItem::operator==::otherHas` | no | low | local fits | keep current | — |
| `NodeMapItem::toJson::obj` | no | low | json::object accumulator | keep current | — |
| `NodeMapItem::toJson::typeVal` | unsure | low | "size" key written from this | `sizeVal` | — |
| `NodeMapItem::toJson::idx` | no | low | typeIdx-1 lookup index | keep current | — |
| `NodeMapItem::toJson::typeTable` | yes | medium | populates JSON `"size"` field | `sizeTable` | — |
| `NodeMapItem::size::idx` | no | low | typeIdx-1 lookup index | keep current | — |
| `NodeMapItem::size::sizeTable` | no | medium | function returns size_t "size" | keep current | not-misnomer |
| `addDirect` (free helper) | no | medium | inserts a DirectAddrNodeMapData entry | keep current | — |
| `addDirect::m` / `key` / `addr` / `typeIdx` / `hasFast` / `fastAddr` | no | low | mirror NodeMapItem fields verbatim | keep current | — |
| `addVirt` (free helper) | no | medium | inserts a VirtAddrNodeMapData entry | keep current | — |
| `addVirt::name` / `addresses` | no | low | mirror VirtAddrNodeMapData fields | keep current | — |
| `Map` (using alias in anon ns) | no | low | local alias for std::map | keep current | — |
| `dispatchHighDevType::devType` | no | low | parameter holds AwgDeviceType | keep current | — |
| `getNodeMapForDevice::devType` | no | low | parameter holds AwgDeviceType | keep current | — |

## 3. Detailed findings

### NodeTypeIdx  [unsure / low / —]

Evidence:
- `node_map_data.hpp:25-34` — declaration with comments
  `IntegerPassthrough = 0  // direct register/int value`,
  `SinePair = 1  // dual 32-bit I+Q`, `Frequency = 4 ... toFrequency()`,
  `Phase = 5 ... toPhase()`.
- `notes/magic_numbers_proposal.md:167` lists the same enum as a *proposal*
  (i.e. invented, not extracted from binary).
- `nm --demangle _seqc_compiler.so | grep NodeTypeIdx` → no matches. The
  enum is reconstruction-side only.
- `node_map_data.cpp:191-200` and the entire `get_node_map.cpp` use a
  bare `int32_t` for the same field; the enum itself is **not** referenced
  anywhere in the codebase (`grep NodeTypeIdx reconstructed/` shows only
  the declaration and notes).
- The wrapping struct member is named `typeIdx` (singular slot for the
  same value), and the bounds checks throughout are written as
  `static_cast<uint32_t>(node.typeIdx) > 5u`, suggesting the consumer
  thinks of it as a small integer code, not a typed enum.

Interpretation:
- `NodeTypeIdx` is a reconstruction-introduced enum that is declared but
  never used. The name itself ("type-index") is a poor fit for what the
  enumerators actually represent — a *value-encoding kind* (integer,
  float bits, double low/high, fixed-point frequency, fixed-point phase).
  "Idx" implies a position/offset; "Encoding" or "Kind" would describe
  the dispatch role more accurately.

Judgement:
- The name is plausible but sub-optimal; without authoritative evidence
  either way, keeping it is acceptable.

Proposals:
- keep current  (medium)
- `NodeValueEncoding`  (low)
- `NodeValueKind`  (low)

Locations consulted:
- declared: include/zhinst/runtime/node_map_data.hpp:25-34
- referenced: notes/magic_numbers_proposal.md:167 (origin); not referenced
  in any .cpp.

### NodeTypeIdx::IntegerPassthrough  [no / medium / not-misnomer]

Evidence:
- `custom_functions_play.cpp:2138` — case 0 takes a "common tail"
  with raw `suser` + `addi` (no value transform).
- `node_map_data.cpp:191-200` — typeIdx 0 falls through the
  out-of-range arm and returns size 1 (single 32-bit slot).

Interpretation:
- Case 0 emits the value with no scaling/encoding, matching the
  "passthrough" connotation.

Judgement:
- Name fairly describes observed dispatch.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/runtime/node_map_data.hpp:28
- used (dispatch): src/runtime/custom_functions_play.cpp:2138, 1604, 1779, 1914
  (case 0 arms — surveyed only).

### NodeTypeIdx::SinePair  [unsure / low / —]

Evidence:
- `node_map_data.hpp:29` comment: `dual 32-bit I+Q (suser 0x17 + 0x19)`.
- `node_map_data.cpp:181` — typeIdx 1 → JSON `size` value of 1
  (single 32-bit slot in JSON), which conflicts with "I+Q pair".
- `node_map_data.cpp:196` — typeIdx 1 → byte size 1 (same conflict).

Interpretation:
- The reconstructed comment in the header asserts I+Q encoding, but the
  two lookup tables (one for `"size"` JSON, one for `size()` accessor)
  both return `1` for typeIdx 1, which would be wrong if it really
  represented a dual-32 pair. Either the comment is wrong, or the
  lookup tables encode something other than "number of 32-bit words".

Judgement:
- Name might be wrong; could equally well be plain int with a marker.
  Insufficient evidence either way.

Proposals:
- keep current  (low)
- `IqPair`  (low)

Locations consulted:
- declared: include/zhinst/runtime/node_map_data.hpp:29
- contradicting tables: src/runtime/node_map_data.cpp:181, 196

### VirtAddrNodeMapData::addresses_  [unsure / medium / —]

Evidence:
- `node_map_data.cpp:80-90`:
  ```cpp
  obj["name"] = boost::json::string_view(name_.data(), name_.size());
  ...
  obj["index"] = std::move(arr);   // string literal "index" at 8fe799
  ```
- `get_node_map.cpp:764-836` — every call site passes a small list of
  oscillator indices, e.g. `addVirt(m, "oscs/7/freq", "OSCFREQ", {7}, 4, ...)`,
  `addVirt(m, "sines/7/phaseshift", "SINEPHASE", {7}, 5, ...)`.
- The JSON serializer emits the field under the key **"index"**, not
  "addresses".

Interpretation:
- The field is serialized as JSON `"index"` and stores small ordinal
  indices into a virtual address space (oscillator number, sine
  channel, etc.). The current name `addresses_` reflects how the
  consumer (the address-resolution logic) interprets the contents,
  but the JSON-key evidence (tier-2 positive evidence per RULES §4d)
  points at `index_` / `indices_` as the binary's name.

Judgement:
- Plausible misnomer; the JSON key is strong but not decisive evidence
  that the C++ field name matches.

Proposals:
- `index_`  (medium)
- `indices_`  (low)
- keep current  (low)

Locations consulted:
- declared: include/zhinst/runtime/node_map_data.hpp:70
- written: src/runtime/get_node_map.cpp (~566 + 252 + … data-table call sites
  via `addVirt(..., addresses, ...)`)
- read:    src/runtime/node_map_data.cpp:42 (compareEq), :55 (hash), :85 (getJson)

### VirtAddrNodeMapData::name_  [no / high / not-misnomer]

Evidence:
- `node_map_data.cpp:80-83`:
  `obj["name"] = boost::json::string_view(name_.data(), name_.size());`
- `get_node_map.cpp` uses tag strings `"OSCFREQ"`, `"OSCPHASERST"`,
  `"SINEHARM"`, `"SINEOSCSEL"`, `"SINEPHASE"`, `"LIOSCFREQ"` at the
  `name` argument to `addVirt`.

Interpretation:
- Field is verbatim serialized as JSON `"name"` (tier-2 positive
  evidence). Holds a node-class tag string (e.g. `"OSCFREQ"`).

Judgement:
- Name is correct.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/runtime/node_map_data.hpp:69
- read:     src/runtime/node_map_data.cpp:42, 49, 82

### kGoldenRatioHash  [no / medium / not-misnomer]

Evidence:
- `node_map_data.cpp:53,107`:
  `constexpr size_t kGoldenRatioHash = 0x9e3779b9ULL;`
- The value `0x9e3779b9` is the lower 32 bits of `2^32 / φ`, the
  canonical "golden ratio hash" / `boost::hash_combine` magic constant.

Interpretation:
- Name accurately identifies the constant.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

### kMul  [unsure / low / —]

Evidence:
- `node_map_data.cpp:54,108`:
  `constexpr size_t kMul = 0x0e9846af9b1a615dULL;`
- Used as the multiplier in two splitmix-style rounds in both `hash`
  implementations.

Interpretation:
- Generic name `kMul` does not say what kind of multiplier. The
  surrounding code is a splitmix64 finalizer.

Judgement:
- Slightly non-descript; rename would be a readability improvement.

Proposals:
- `kSplitmixMul`  (low)
- keep current  (low)

### DirectAddrNodeMapData::addr_  [no / high / not-misnomer]

Evidence:
- `node_map_data.cpp:127-129`:
  `obj["direct_address"] = static_cast<std::int64_t>(addr_);`
- `custom_functions.cpp:528-529`:
  `if (auto* direct = dynamic_cast<DirectAddrNodeMapData*>(item.data))
       return direct->addr_;`
- `get_node_map.cpp:73…` hundreds of `addDirect(m, "...", 0xNNNN, ...)`
  calls passing literal hardware addresses like `0xe804`, `0xea14`, etc.

Interpretation:
- Field is serialized as JSON `"direct_address"` and is the literal
  hardware register address used for direct-address dispatch.

Judgement:
- Name is correct.

Proposals:
- keep current  (high)

### NodeMapItem::data  [no / medium / not-misnomer]

Evidence:
- `node_map_data.hpp:107` declared as `NodeMapData* data`.
- `node_map_data.cpp:153-155` — typeid + virtual-dispatch via
  `data->compareEq(*other.data)`.
- `node_map_data.cpp:171` — `data->getJson(obj);`
- `custom_functions_play.cpp:1550` — `dynamic_cast<DirectAddrNodeMapData*>(node.data)`.

Interpretation:
- The field's type literally is `NodeMapData*`; "data" is the only
  non-redundant short name (a name like `nodeMapData` would just
  duplicate the type).

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

### NodeMapItem::typeIdx  [unsure / low / —]

Evidence:
- `node_map_data.hpp:108` declared as `int32_t typeIdx`.
- All consumer sites use it as a dispatch tag bounded ≤ 5:
  - `custom_functions_play.cpp:1543,1555,1569,1582`:
    `static_cast<uint32_t>(node.typeIdx) > 5u`.
  - `custom_functions_play.cpp:2138`:
    `if (node.typeIdx == 0 || node.typeIdx == 1 || node.typeIdx == 5)`.
  - `node_map_data.cpp:177,192`: `unsigned idx = static_cast<unsigned>(typeIdx) - 1;`
- `get_node_map.cpp:73…`: every addDirect/addVirt write site passes
  literal small constants (0, 2, 4, 5) for this argument; never an
  index-shaped value.

Interpretation:
- The field is a small enumerated **kind/code**, not an index into
  anything. The "Idx" suffix is misleading; "code", "kind", "tag" or
  "encoding" would describe the role better. (The field type itself —
  `int32_t` vs the unused `NodeTypeIdx` enum — is a separate type-side
  observation, recorded here per RULES §2a.)

Judgement:
- Plausible misnomer; weight is low because the existing name has
  evidently sufficed for years and the suffix "Idx" is at worst
  imprecise rather than wrong.

Proposals:
- keep current  (medium)
- `typeCode`  (low)
- `valueKind`  (low)

Locations consulted:
- declared: include/zhinst/runtime/node_map_data.hpp:108
- written:  src/runtime/get_node_map.cpp (every call — ~1080 entries)
- read:     src/runtime/node_map_data.cpp:140,177,192;
            src/runtime/custom_functions_play.cpp:1543,1555,1569,1582,1604,1779,1914,2138

### NodeMapItem::hasFast  [yes / medium / cross-batch-arbitration]

Evidence:
- `node_map_data.hpp:110`: `bool hasFast;` with comment
  `+0x10  1  bool  hasFast  (has fast address)`.
- `node_map_data.cpp:159-161`:
  `uint32_t fastAddress() const { return hasFast ? fastAddr : 0; }`
- `custom_functions_play.cpp:1452-1456` — explicit reconstruction
  comment: *"a byte from NodeMapItem+0x10 — the SAME slot we model
  as the `hasFast` bool. This means either (a) the field is
  overloaded and AccessMode is encoded as a 0/1 byte that doubles
  as a hasFast indicator, or (b) hasFast IS the AccessMode and
  our typing is wrong."*
- `custom_functions_play.cpp:1511-1512`:
  `AccessMode accessMode = static_cast<AccessMode>(node.hasFast);
   addNodeAccess(node, accessMode);`
- `addNodeAccess`'s 2nd parameter type is `AccessMode` (a 3-valued
  enum: Soft=0, Direct=1, Custom=2) — see
  `include/zhinst/runtime/custom_functions.hpp:370`.
- `get_node_map.cpp` — every entry that sets the slot to `true`
  also sets a non-zero `fastAddr` (lines 764-836). Entries that
  pass `true` look like normal oscillator/sine direct-write nodes
  whose access semantics could plausibly be `Direct` (=1 in
  AccessMode).
- `node_map_data.cpp:143-150` — `operator==` treats it as a bool:
  if both `hasFast`, compare `fastAddr`; if mismatch, unequal.

Interpretation:
- The same byte at `+0x10` is read as both (a) a 0/1 boolean gating
  fast-path dispatch and (b) the `AccessMode` argument to
  `addNodeAccess`. This is the inconsistency itself; which name is
  wrong is a separate question. If the byte's semantic is
  "AccessMode" (with `Direct=1` and `Soft=0` happening to match the
  binary's gating choice), then `hasFast` is the misnomer. If the
  field really is just a flag and `addNodeAccess` is being called
  with a degenerate cast, then `hasFast` is correct and the cast
  in `custom_functions_play.cpp:1511` is the bug.

Judgement:
- The conflict is high-confidence; which side is wrong is a
  lower-confidence judgement that should be resolved together with
  the `AccessMode`/`addNodeAccess` side (likely batch 33 or wherever
  CustomFunctions::addNodeAccess and AccessMode live; flagged
  cross-batch).

Proposals:
- keep current  (low)         — if the cast at play.cpp:1511 is the bug
- `accessModeRaw` (uint8_t)  (medium) — if the AccessMode interpretation is correct
- `accessOrFastFlag`  (low)   — neutral while disambiguation is pending

Cross-reference:
- Counterpart: `CustomFunctions::addNodeAccess::mode` parameter and
  `AccessMode` enum (custom_functions audit batch — `nm` confirms
  `addNodeAccess(NodeMapItem const&, AccessMode)` exists in the
  binary at @0x15c6c0).

Locations consulted:
- declared:   include/zhinst/runtime/node_map_data.hpp:110
- read:       src/runtime/node_map_data.cpp:144, 145, 160;
              src/runtime/custom_functions_play.cpp:1511, 1536
- written:    src/runtime/get_node_map.cpp:38,51 (helpers); 764-836 (true sites)

### NodeMapItem::fastAddr  [no / medium / not-misnomer]

Evidence:
- `node_map_data.hpp:109`: `uint32_t fastAddr; // +0x0C`.
- `node_map_data.cpp:160`:
  `return hasFast ? fastAddr : 0;`
- `custom_functions_play.cpp:1538`:
  `addr = node.fastAddress();  // path A: jt @958f68`
- `get_node_map.cpp:764-836` — written with literal small offsets
  (0x00, 0x08, 0x10, …, 0x88, 0x9c, 0xa0, 0xa4, 0xa8, 0xac, 0xb0)
  that look like an SSE-style register-file offset.

Interpretation:
- Method name `fastAddress` is in the binary symbol table and selects
  this field; the field is the storage backing the "fast-path"
  address dispatch.

Judgement:
- Name matches usage and the binary-confirmed accessor name.

Proposals:
- keep current  (high)

### NodeMapItem::toJson::typeTable  [yes / medium / —]

Evidence:
- `node_map_data.cpp:181-185`:
  ```cpp
  static const std::int64_t typeTable[4] = {2, 1, 2, 2};
  typeVal = typeTable[idx];
  ...
  obj["size"] = typeVal;
  ```

Interpretation:
- The local table populates the JSON `"size"` field. Its values
  literally represent sizes (in 32-bit words: int=2, sine-pair=1,
  float=2, double-low=2). Naming it `typeTable` obscures this; the
  parallel local `sizeTable` in `NodeMapItem::size` (line 196) has
  identical contents and the (correct) name.

Judgement:
- Local name disagrees with both its content and its sibling
  (`size::sizeTable`) for no reason.

Proposals:
- `sizeTable`  (medium)
- `sizeByType` (low)

Locations consulted:
- declared/used: src/runtime/node_map_data.cpp:181-185
- sibling reference: src/runtime/node_map_data.cpp:196

### VirtAddrNodeMapData::compareEq::o and DirectAddrNodeMapData::compareEq::o  [yes / low / —]

Evidence:
- `node_map_data.cpp:41-42`, `:101-102`:
  ```cpp
  auto const& o = static_cast<VirtAddrNodeMapData const&>(other);
  return name_ == o.name_ && addresses_ == o.addresses_;
  ```

Interpretation:
- One-letter local in a context where the standard convention
  `rhs` or `otherCast` would be clearer; trivial.

Judgement:
- Mildly misleading but low-impact.

Proposals:
- `rhs`  (low)
- `otherCast`  (low)

### DirectAddrNodeMapData::clone::p  [yes / low / —]

Evidence:
- `node_map_data.cpp:117-121`:
  ```cpp
  auto* p = new DirectAddrNodeMapData();
  p->addr_ = addr_;
  return p;
  ```

Interpretation:
- One-letter local. `copy` or `cloned` would describe the role.

Judgement:
- Minor readability blemish.

Proposals:
- `copy`  (low)
- `cloned`  (low)

### NodeMapItem::toJson::typeVal  [unsure / low / —]

Evidence:
- `node_map_data.cpp:176-185`:
  ```cpp
  std::int64_t typeVal = 1;
  ...
  typeVal = typeTable[idx];
  ...
  obj["size"] = typeVal;
  ```

Interpretation:
- The variable is assigned to JSON `"size"`. The current name
  `typeVal` describes provenance ("value for the type"), not role.

Judgement:
- Mildly inconsistent with the JSON key it populates.

Proposals:
- `sizeVal`  (low)
- `sizeWords`  (low)

## 4. Symbols inspected and judged routinely fine

- `NodeMapData` / `VirtAddrNodeMapData` / `DirectAddrNodeMapData` /
  `NodeMapItem` — type names, present in `nm --demangle` (out of scope
  per RULES §3).
- All five virtual-method names (`compareEq`, `hash`, `clone`,
  `getJson`, destructor) — present in `nm` (out of scope).
- `NodeMapItem::fastAddress`, `toJson`, `size`, `operator==` — present
  in `nm` (out of scope).
- `compareEq::other` (both subclasses) — overrides a virtual whose base
  signature uses `other`; conventional.
- `getJson::obj` (both subclasses) — out-parameter for JSON object;
  unambiguous.
- `hash::seed`, `hash::v`, `hash::combined`, `hash::h`, `hash::a`
  (Virt) and `hash::v` (Direct) — splitmix scratch locals; standard.
- `NodeMapItem::operator==::other`, `::thisHas`, `::otherHas` — fit
  the comparison logic.
- `NodeMapItem::pad_11` — explicit padding member, name matches offset.
- `addDirect` / `addVirt` parameter names (`m`, `key`, `addr`, `name`,
  `addresses`, `typeIdx`, `hasFast`, `fastAddr`) — all mirror their
  destination NodeMapItem/NodeMapData fields verbatim; routine
  pass-through.
- `Map` (using-alias inside anonymous namespace of `get_node_map.cpp`)
  — local convenience; no externally visible role.
- `dispatchHighDevType::devType` and `getNodeMapForDevice::devType` —
  parameters typed `AwgDeviceType`; name matches type.
- `NodeMapItem::size::idx`, `NodeMapItem::toJson::idx` — `typeIdx-1`
  lookup index; fits.
- `NodeMapItem::size::sizeTable` — table populates a function
  literally named `size()`; name matches role.

## 5. Coverage

**Fully covered:**
- `include/zhinst/runtime/node_map_data.hpp` — every in-scope symbol
  (enum + enumerators, fields of all three classes, padding,
  parameter names of all method declarations).
- `src/runtime/node_map_data.cpp` — every in-scope local, parameter, and
  constant.
- `src/runtime/get_node_map.cpp` — fully scanned at the symbol level. Beyond
  the two anonymous-namespace helpers `addDirect` / `addVirt`, their
  parameter names, the `Map` alias, and the two free functions
  (`dispatchHighDevType`, `getNodeMapForDevice`) and their
  `devType` parameters, the file contains **no further nameable
  symbols** — it is ~1080 lines of literal data-table entries
  (`addDirect(m, "path", 0xADDR, code, ...)` and
  `addVirt(m, "path", "TAG", {indices}, code, ...)`). The string
  literals (node paths, tag names) are data, not symbols.

**Deferred:** none.

**Not covered (out of scope per RULES §2/§3):**
- Type names `NodeMapData`, `VirtAddrNodeMapData`,
  `DirectAddrNodeMapData`, `NodeMapItem`. Confirmed in
  `nm --demangle _seqc_compiler.so`:
  `00000000001c5720 t zhinst::NodeMapData::~NodeMapData()`,
  `0000000000b04d18 d typeinfo for zhinst::VirtAddrNodeMapData`,
  `0000000000b04d70 d typeinfo for zhinst::DirectAddrNodeMapData`,
  `zhinst::NodeMapItem::operator==(zhinst::NodeMapItem const&) const`.
- All virtual and non-virtual method names listed in §4 — same `nm`
  output.
- Template `GetNodeMap` and its specializations — class template name
  not recorded in `nm --demangle` output for the .so (the binary
  inlines/specialises directly), but this file contains only one
  `GetNodeMap` symbol (the class template head) which is a
  reconstruction-side scaffold; the per-device specializations are
  bound by the explicitly-instantiated `static get()` methods, none
  of which appear in `nm`. Since the type is reconstruction-side,
  it could in principle be in scope, but with only one declaration
  and no consumer-facing role it is left as a no-finding.
- `NodeMap`, `NodeMap::entries_` — declared in
  `include/zhinst/runtime/custom_functions.hpp` (batch 33's territory); only
  *used* here. Not in scope for this batch.
- `AwgDeviceType` and its enumerators — type lives in
  `include/zhinst/core/types.hpp`; not in scope for this batch.
- `AccessMode` and `addNodeAccess` — counterparts to the
  `NodeMapItem::hasFast` arbitration; live in
  `include/zhinst/runtime/custom_functions.hpp` (batch 33).
