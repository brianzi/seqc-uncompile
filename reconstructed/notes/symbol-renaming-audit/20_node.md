# Batch 20 — node

## 1. Files considered

- `reconstructed/include/zhinst/node.hpp`
- `reconstructed/src/node.cpp`

Cross-batch context consulted (read-only):

- `reconstructed/src/prefetch.cpp` (Node construction sites, asmId/nodeId
  reads, loadRef reads)
- `reconstructed/src/prefetch_print.cpp` (print() — exhaustive use of
  asmId, type, config, currentCwvf, globalRate, loadRef, play, branches,
  loop, next)
- `reconstructed/src/prefetch_emit.cpp` (currentCwvf field reads,
  loopBodyRunsAtLeastOnce, parent.lock())
- `reconstructed/src/prefetch_placesingle.cpp` (length, loadRef)
- `reconstructed/src/prefetch_splitplay.cpp` (length, loadRef)
- `reconstructed/src/prefetch_prepare.cpp` (length)
- `reconstructed/src/asm_commands.cpp` (Node ctor call site;
  `asmId = result.sequenceId`, `numWaveSlots = numChannelGroups_`)
- `reconstructed/src/asm_list.cpp` (nodeId read at @0x267808)
- `reconstructed/src/custom_functions_io.cpp` (only `config.deviceIndex`
  — `AWGCompilerConfig` field, not a Node field, so unrelated)
- `reconstructed/include/zhinst/error_messages.hpp` (PrefetchError,
  SwapNotConnected — namespace-scope enumerators owned by another batch)
- `reconstructed/notes/symbol-renaming-audit/27_node_map_data.md`
  (sister batch — adjacent territory; cross-checked naming style)

`nm --demangle _seqc_compiler.so | grep "zhinst::Node"` was used as the
authoritative excluder for the type name and methods (see §5).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Node::idCounter_` | no | high | mangled static symbol present | keep current | not-misnomer |
| `Node::nodeId` | no | medium | TLS counter, used as ID | keep current | not-misnomer |
| `Node::asmId` | no | medium | matches `result.sequenceId` source | keep current | not-misnomer |
| `Node::loadRef` | no | medium | locked to obtain load `Node` | keep current | not-misnomer |
| `Node::wavesPerDev` | no | high | JSON key `"wavesPerDev"` verbatim | keep current | not-misnomer |
| `Node::deviceIndex` | no | high | JSON key `"deviceIndex"` verbatim | keep current | not-misnomer |
| `Node::type` | no | high | JSON key `"type"`; ubiquitous | keep current | not-misnomer |
| `Node::config` | no | high | JSON key `"config"` verbatim | keep current | not-misnomer |
| `Node::currentCwvf` | no | high | JSON key `"currentCwvf"` verbatim | keep current | not-misnomer |
| `Node::lengthReg` | no | high | JSON key `"lengthReg"` verbatim | keep current | not-misnomer |
| `Node::length` | no | medium | play-length sample count usage | keep current | not-misnomer |
| `Node::indexOffsetReg` | no | high | JSON key verbatim | keep current | not-misnomer |
| `Node::tableIndex` | no | high | JSON key verbatim | keep current | not-misnomer |
| `Node::play` | no | high | JSON key `"play"` verbatim | keep current | not-misnomer |
| `Node::next` | no | high | JSON key `"next"`; sibling chain | keep current | not-misnomer |
| `Node::branches` | no | high | JSON key `"branches"` verbatim | keep current | not-misnomer |
| `Node::loop` | no | high | JSON key `"loop"` verbatim | keep current | not-misnomer |
| `Node::parent` | no | high | JSON key `"parent"` verbatim | keep current | not-misnomer |
| `Node::globalRate` | no | high | JSON key `"globalRate"` verbatim | keep current | not-misnomer |
| `Node::defaultPrecompFlags` | no | high | JSON key verbatim | keep current | not-misnomer |
| `Node::loopBodyRunsAtLeastOnce` | no | high | JSON key verbatim | keep current | not-misnomer |
| `Node::branchMaySkipAllBodies` | no | high | JSON key verbatim | keep current | not-misnomer |
| `Node::trig` | unsure | low | JSON `"trig"`; usage minimal | keep current; `trigger` | — |
| `NodeType` (enum) | no | high | mangled as `Node::NodeType` | keep current | not-misnomer |
| `NodeType::Load`..`AwgReady` | no | high | str2type/type2str strings verbatim | keep current | not-misnomer |
| `NodeType::SetVarPlaceholder`..`Wait` | unsure | low | not in str2type/type2str | keep current | verify-not-original |
| `Node::Node(simple)::type` | no | high | stored into `type` | keep current | — |
| `Node::Node(simple)::asmId` | no | high | stored into `asmId` | keep current | — |
| `Node::Node(simple)::numWaveSlots` | no | medium | sized to `wavesPerDev` from `numChannelGroups` | keep current | not-misnomer |
| `Node::Node(full)::*` | no | medium | mirror of fields | keep current | — |
| `Node::type2str::t` | no | low | one-letter param fits switch | keep current | — |
| `Node::str2type::s` | no | low | one-letter param, lookup key | keep current | — |
| `Node::last::node` | no | low | walks `node->next` chain | keep current | — |
| `Node::insertBefore::newNode` | no | high | becomes new previous sibling | keep current | not-misnomer |
| `Node::insertBefore::par` | no | low | locked parent shared_ptr | keep current | — |
| `Node::updateParent::parent` | no | medium | the parent being mutated | keep current | not-misnomer |
| `Node::updateParent::oldChild` | no | medium | child being replaced | keep current | not-misnomer |
| `Node::updateParent::newChild` | no | medium | replacement child | keep current | not-misnomer |
| `Node::updateParent::ch` | yes | low | one-letter alias for `branches` | `branches` | — |
| `Node::updateParent::it` | no | low | iterator | keep current | — |
| `Node::remove::node` | no | low | parameter being removed | keep current | — |
| `Node::remove::par` | no | low | locked parent | keep current | — |
| `Node::remove::child` | no | low | branch element | keep current | — |
| `Node::swap::a` / `::b` | no | low | matches static method signature | keep current | — |
| `Node::swap::bNode` / `aNode` | no | low | raw `Node*` aliases | keep current | — |
| `Node::swap::bParentCtrl` | no | low | locked weak_ptr | keep current | — |
| `Node::swap::bParentRaw` / `aRaw` | no | low | raw pointers for compare | keep current | — |
| `Node::swap::current` | no | low | walker over Loop/Branch ancestors | keep current | — |
| `Node::swap::cur` | no | low | raw alias | keep current | — |
| `Node::swap::t` | no | low | ancestor type code | keep current | — |
| `Node::swap::parentLocked` | no | low | locked ancestor parent | keep current | — |
| `Node::swap::ancestor` | no | medium | non-Loop/Branch ancestor | keep current | not-misnomer |
| `Node::swap::devIdx` | yes | medium | reads `asmId`, name says deviceIndex | `asmIdToCopy`; `ancestorAsmId` | — |
| `Node::swap::parentOfA` | no | medium | matches role | keep current | not-misnomer |
| `Node::swap::locked` | no | low | locked weak_ptr | keep current | — |
| `Node::swap::bNext` | no | high | saved `b->next` | keep current | not-misnomer |
| `throwSwapNotConnected` (free fn) | no | medium | fits sole call sites | keep current | — |
| `throwSwapNotConnected::errStr`/`formatted` | no | low | obvious roles | keep current | — |
| `Node::clone::n` | no | low | one-letter local for the new node | keep current | — |
| `Node::toJson::idMap` | unsure | low | also looked up by `asmId` | keep current; `seqIdToJsonId` | — |
| `Node::toJson::wavesFlat` | no | low | flattened optionals | keep current | — |
| `Node::toJson::opt` | no | low | range-for iter | keep current | — |
| `Node::toJson::playIds` | no | low | locked weak_ptr id list | keep current | — |
| `Node::toJson::wp`/`sp` | no | low | weak/shared aliases | keep current | — |
| `Node::toJson::branchIds`/`child` | no | low | branch id list | keep current | — |
| `Node::toJson::remappedId` | unsure | low | used for `"nodeId"` JSON; lookup-by-asmId | keep current; `nodeIdJson` | — |
| `Node::toJson::nextId`/`loopId`/`parentId` | no | medium | match JSON keys | keep current | — |
| `Node::toJson::lengthRegVal` / `indexOffsetRegVal` | no | medium | int-encoded register payload | keep current | — |
| `Node::toJson::wavesArr`/`branchArr`/`playArr`/`result` | no | low | local builders | keep current | — |
| `Node::fromJson::json` | no | low | the input JSON value | keep current | — |
| `Node::fromJson::obj` | no | low | json::object alias | keep current | — |
| `Node::fromJson::wavesJsonArr`/`waves`/`elem` | no | low | obvious roles | keep current | — |
| `Node::fromJson::nId`/`aId`/`devIdx` | unsure | low | abbreviations of field names | `nodeIdJson`/`asmIdJson`/`deviceIndex`; keep current | — |
| `Node::fromJson::typeStr`/`typeStdStr`/`nodeType` | no | low | strings → enum | keep current | — |
| `Node::fromJson::cfg1`/`cfg2` | unsure | low | `config` and `currentCwvf` payloads | `config`/`currentCwvf`; keep current | — |
| `Node::fromJson::lengthRegVal`/`lReg`/`lengthField` | no | low | parsed values | keep current | — |
| `Node::fromJson::idxOffsetRegVal`/`ioReg` | no | low | parsed values | keep current | — |
| `Node::fromJson::tableIdx`/`gRate`/`precompFlags`/`loopRuns`/`branchSkips`/`trigField` | no | low | parsed values | keep current | — |
| `Node::fromJson::node` | no | low | constructed result | keep current | — |
| `Node::installPointers::nodeMap` | no | medium | maps id → node | keep current | not-misnomer |
| `Node::installPointers::json` | no | low | source object | keep current | — |
| `Node::installPointers::localMap` | no | low | binary copies map locally | keep current | — |
| `Node::installPointers::lookupNode` (lambda) | no | medium | id → node lookup helper | keep current | not-misnomer |
| `Node::installPointers::v`/`id`/`it` | no | low | lambda locals | keep current | — |
| `Node::installPointers::obj` | no | low | json::object alias | keep current | — |
| `Node::installPointers::playArr`/`elem`/`branchesArr` | no | low | array iteration | keep current | — |
| `Node::installPointers::nextVal`/`loopVal`/`parentVal`/`oldNext`/`oldLoop`/`sp` | no | low | obvious | keep current | — |

(Method names `Node`, `~Node`, `type2str`, `str2type`, `toString`,
`waveAtCurrentDeviceIndex`, `last`, `insertBefore`, `updateParent`,
`remove`, `swap`, `clone`, `toJson`, `fromJson`, `installPointers`
are out of scope — see §5.)

## 3. Detailed findings

### Node::idCounter_  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so`:
  `0000000000000044 b zhinst::Node::idCounter_`
  (TLS BSS template offset 0x44).
- `node.cpp:22`: `thread_local int Node::idCounter_ = 0;`
- `node.cpp:46`: `, nodeId(idCounter_++)` — read-and-post-increment.

Interpretation:
- The mangled symbol `zhinst::Node::idCounter_` is in the binary's
  symbol table as a `static` data member. Per RULES §3, static data
  members are tier-1 authoritative and excluded from rename.

Judgement:
- Name comes from the binary; not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/node.hpp:238
- defined:  src/node.cpp:22
- used:     src/node.cpp:46

### Node::nodeId  [no / medium / not-misnomer]

Evidence:
- JSON key `"nodeId"` written in `toJson` (node.cpp:550) and read in
  `fromJson` (node.cpp:599). String is a verbatim .rodata entry per
  the header table.
- Set from TLS counter in the simple ctor (node.cpp:46).
- Read consistently as the per-node identifier when remapping
  pointer fields to ids in `toJson` (node.cpp:511, 517, 523, 491,
  499) and as the lookup key for the `nodeMap` of the
  `installPointers` API (node.cpp:660).
- `asm_list.cpp:564-565`: `int nodeSeqId = node->nodeId;  // +0x10`
  — used as a sequence identifier into the install-pointer map.

Interpretation:
- Field is consistently the node's unique identity within an AST
  serialization, sourced from a TLS counter. Name matches usage and
  the .rodata JSON key.

Judgement:
- Name fits.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/node.hpp:245
- written:  src/node.cpp:46
- read:     src/node.cpp:491,499,511,517,523; src/asm_list.cpp:565

### Node::asmId  [no / medium / not-misnomer]

Evidence:
- JSON key `"asmId"` (node.cpp:551, fromJson at 600) — verbatim from
  the .rodata table in the header.
- Construction site in `asm_commands.cpp:79`:
  `result.node = std::make_shared<Node>(type, result.sequenceId,
   numChannelGroups_);` — the second ctor arg (asmId) receives the
  asm-list `sequenceId` directly.
- Read as the printed "asmID" label in `prefetch_print.cpp` at many
  sites (e.g. lines 68, 114, 135, 218, 227, 306, 413, 452, 466, 508,
  521).
- `prefetch.cpp:753-755`, `:1115`, `:1132`, `:1319`, `:1363`,
  `:2098-2099`, `:2285`, `:2298` — copied between nodes during
  swap/clone-style operations to keep the asm-list anchor stable.
- The "asm id" appears verbatim in `Node::toString()` output:
  `"Node (asm id N, type T)"` (node.cpp:197).

Interpretation:
- Field is the asm-list sequence identifier of the asm command that
  produced or anchors this node. The name encodes that role precisely
  and is reflected verbatim in JSON, in `toString`, and in the
  printed prefetch trees.

Judgement:
- Name fits.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/node.hpp:246
- written:  src/asm_commands.cpp:79; src/prefetch.cpp:1115,1132,1319,1363
- read:     src/node.cpp:197,551,600; src/prefetch_print.cpp (many);
            src/prefetch.cpp:753,2098

### Node::loadRef  [no / medium / not-misnomer]

Evidence:
- Header comment block at node.hpp:248-261 enumerates the four
  source aliases (`loadNode`, `loadPtr`, `load`, `loadRef`) and
  declares the canonical name `loadRef`.
- `prefetch_print.cpp:102,191,392`:
  `std::shared_ptr<Node> loadNode = n->loadRef.lock();`
- `prefetch_placesingle.cpp:478,1024`:
  `std::shared_ptr<Node> loadNode = np->loadRef.lock();`
- `prefetch_splitplay.cpp:126`:
  `std::shared_ptr<Node> lookupNode = raw->loadRef.lock();`
- All read sites lock the weak_ptr to get the actual Load `Node`.

Interpretation:
- Field is a `std::weak_ptr<Node>` that points back to the Load node
  associated with this Play/CWVF node. "Ref" reflects the weak-ptr
  nature; the locked result is named `loadNode` everywhere, which is
  consistent with the field representing a *reference to* the load.

Judgement:
- Name fits and is intentionally distinct from the locked
  `loadNode` shared_ptr.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/node.hpp:261
- read:     src/prefetch_print.cpp:102,191,392;
            src/prefetch_placesingle.cpp:478,1024;
            src/prefetch_splitplay.cpp:126

### JSON-key-anchored fields (one block, multiple symbols) [no / high / not-misnomer]

This block covers fields whose names appear verbatim in the binary
.rodata as JSON serializer keys (tier-2 positive evidence per
RULES §4d). They are listed together because the evidence form is
identical for all of them:

Symbols:
- `wavesPerDev`, `deviceIndex`, `type`, `config`, `currentCwvf`,
  `lengthReg`, `indexOffsetReg`, `tableIndex`, `play`, `next`,
  `branches`, `loop`, `parent`, `globalRate`,
  `defaultPrecompFlags`, `loopBodyRunsAtLeastOnce`,
  `branchMaySkipAllBodies`.

Evidence:
- `node.hpp:84-114` — Each field is annotated with its JSON key; the
  `.rodata` addresses are listed in the `toJson` header comment
  (node.cpp:452-473).
- `node.cpp:549-571` — `toJson()` writes each as a key-value pair
  using the same string literal.
- `node.cpp:585-622` — `fromJson()` reads each by the same key.
- All field names also appear verbatim in usage code (e.g.
  `n->branches`, `n->next`, `n->loop`, `n->parent`, `n->globalRate`,
  `n->currentCwvf.*`, `prev->loopBodyRunsAtLeastOnce`).

Interpretation:
- The JSON serializer key strings are present verbatim in the binary
  `.rodata`, written by code at known addresses listed in the header.
  Per RULES §4d tier 2, this is strong positive evidence that the
  C++ field names match the binary's internal naming.

Judgement:
- All names fit — they are the binary's own names as preserved by
  the serializer keys.

Proposals:
- keep current (high) — for each of the 17 fields above.

Locations consulted:
- declared: include/zhinst/node.hpp:84-114, 263-303
- written:  src/node.cpp:549-571
- read:     src/node.cpp:585-622; src/prefetch_emit.cpp (many);
            src/prefetch_print.cpp (many)

### Node::trig  [unsure / low / —]

Evidence:
- Field is at +0x10C, declared `int trig = 0;` (node.hpp:305).
- JSON serialized as `"trig"` (node.cpp:568, fromJson:622).
- `clone()` copies it (node.cpp:231).
- No other reader/writer found in `reconstructed/src/` (`grep
  -rEn "->trig\b|node\.trig\b" reconstructed/src/`: only this file).

Interpretation:
- The JSON key `"trig"` is verbatim from `.rodata`, so the name is
  the binary's serializer name. However, `trig` is also a common
  abbreviation for "trigger", and `PlayConfig::trigger` exists as a
  full-word field next to it (`prefetch_emit.cpp:338,384,402,426`
  reads `currentCwvf.trigger`). The two names belong to different
  structures and need not match.
- No usage outside this file means the field's *role* cannot be
  cross-checked against a consumer.

Judgement:
- The serializer evidence supports the abbreviated name; the lack of
  consumer usage prevents a confident verdict either way. `trigger`
  would be the only plausible alternative.

Proposals:
- keep current (medium) — JSON key is verbatim
- `trigger` (low) — only if the binary actually used `trigger` and
  the serializer abbreviated; no evidence supports this

Locations consulted:
- declared: include/zhinst/node.hpp:305
- read:     src/node.cpp:231,568,622
- written:  src/node.cpp:231,622

### NodeType enumerators (tail not in str2type)  [unsure / low / verify-not-original]

Evidence:
- `node.hpp:44-67` declares 22 `NodeType` enumerators.
- `node.cpp:142-158` (`type2str`) maps only 14 of them to strings
  (Load..AwgReady), with all others falling through to `"unknnown"`.
- `node.cpp:169-184` (`str2type`) likewise only registers those 14.
- The remaining 8 (`SetVarPlaceholder`, `PrecompFlags`,
  `SyncPlaceholderCervino`, `UnlockPlaceholder`, `Placeholder`,
  `Wait`, `LockPlaceholder`, `SyncHirzel` — wait, `SyncHirzel` IS in
  the table at NodeType::SyncHirzel = 0x2000 → "sync_hirzel") so
  the actual unmapped set is: `SetVarPlaceholder`, `PrecompFlags`,
  `SyncPlaceholderCervino`, `UnlockPlaceholder`, `Placeholder`,
  `Wait`, `LockPlaceholder`.
- Names like `Placeholder`, `LockPlaceholder`,
  `UnlockPlaceholder`, `SyncPlaceholderCervino`,
  `SetVarPlaceholder`, `Wait` are descriptive enough to be
  reconstruction-side guesses based on usage in
  `prefetch.cpp`/`prefetch_emit.cpp`. No `.rodata` string anchors
  them.

Interpretation:
- The 14 enumerators that round-trip through `type2str`/`str2type`
  are tier-2 anchored (RULES §4d). The other 7 are reconstruction
  guesses without serializer evidence.

Judgement:
- The 14 anchored names are not misnomers. The 7 unanchored names
  are plausible from usage but cannot be confirmed without a usage
  audit per name. Defer to synthesis.

Proposals:
- keep current (medium) for all
- batch-7-style usage audit per unanchored enumerator (low)

Locations consulted:
- declared: include/zhinst/node.hpp:44-67
- mapped:   src/node.cpp:142-158, 169-184

### Node::Node(simple)::numWaveSlots  [no / medium / not-misnomer]

Evidence:
- `asm_commands.cpp:79`:
  `result.node = std::make_shared<Node>(type, result.sequenceId,
   numChannelGroups_);` — this is the dominant call site.
- `prefetch.cpp:755`: `NodeType::Load, asmId,
   config_->numChannelGroups);`
- `prefetch.cpp:2099`: `make_shared<Node>(loadType, asmId,
   config_->numChannelGroups);`
- `node.cpp:48`: `wavesPerDev(numWaveSlots)` — the parameter sizes
  the `wavesPerDev` vector (one optional<string> per slot).

Interpretation:
- The parameter sizes the `wavesPerDev` vector. The argument passed
  is `numChannelGroups` — i.e. the number of "channel groups" /
  device slots. Calling the formal `numWaveSlots` describes its
  *purpose at the receiver* (number of wave slots in the per-device
  vector); calling it `numChannelGroups` would describe the
  *provenance at the caller*.

Judgement:
- Name describes the role at the receiver and matches the field it
  sizes (`wavesPerDev`, "waves per device"). Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/node.hpp:127
- defined:  src/node.cpp:45-48
- callers:  src/asm_commands.cpp:79; src/prefetch.cpp:755,2099,2284,2297

### Node::swap::devIdx  [yes / medium / —]

Evidence:
- `node.cpp:415-418`:
  ```cpp
  Node* ancestor = current.get();
  int devIdx = ancestor->asmId;        // 0x1d286a: eax = r15->0x14
  if (devIdx > 0) {
      bNode->asmId = devIdx;
  }
  ```
- The value is read from `Node::asmId` (+0x14) and written back into
  `Node::asmId` of the swap target.
- A separate field `Node::deviceIndex` exists at +0x40 (a different
  concept — index into `wavesPerDev`).

Interpretation:
- The local is named `devIdx` but its sole content is the
  ancestor's `asmId`, copied into the partner node's `asmId`. The
  name `devIdx` is associated elsewhere in this codebase with
  `deviceIndex`, an unrelated field. Calling this temporary
  `devIdx` actively suggests a wrong association.

Judgement:
- Name is a misnomer in this local scope.

Proposals:
- `ancestorAsmId` (high)
- `asmIdToCopy` (medium)
- `asmId` (medium) — shadows the field name but the field isn't in
  scope here

Locations consulted:
- declared & used: src/node.cpp:416-418

### Node::updateParent::ch  [yes / low / —]

Evidence:
- `node.cpp:269-279`:
  ```cpp
  auto& ch = parent->branches;
  for (auto it = ch.begin(); it != ch.end(); ++it) {
      if (*it == oldChild) { ... }
  }
  ```

Interpretation:
- `ch` is a one-letter alias for `parent->branches`. Could be read
  as "child" (singular), "channel", or "characters" — none of which
  is what the variable holds (the branches *vector*).

Judgement:
- Name is mildly misleading; trivial readability blemish.

Proposals:
- `branches` (medium) — shadows nothing in this scope
- `siblings` (low)

Locations consulted:
- declared & used: src/node.cpp:269-279

### Node::toJson::idMap  [unsure / low / —]

Evidence:
- `node.cpp:475`: parameter type is
  `const std::unordered_map<int,int>& idMap`.
- `node.cpp:506`: `int remappedId = idMap.at(asmId);  // 0x264fef`
  — the lookup key is **`asmId`**, not `nodeId`.
- The lookup result is then written under JSON key `"nodeId"`
  (node.cpp:550).
- Pointer-field IDs (next/loop/parent/branches/play, lines
  511,517,523,491,499) are emitted as raw `nodeId` values without
  any remapping.

Interpretation:
- The map is keyed by `asmId` and produces values stored under
  `"nodeId"`. A name like `idMap` does not pick a side; the lookup
  pattern reveals an asymmetry that may also be relevant to the
  semantic-bug class (recorded for incidental review). Whether the
  name "idMap" is misleading depends on whether the binary truly
  remaps asmId→jsonNodeId or whether one of the operands is wrong.

Judgement:
- Insufficient evidence to call the name wrong. The asymmetry is
  worth a flag for synthesis.

Proposals:
- keep current (medium)
- `seqIdToJsonId` (low) — would document the asymmetry if real
- `asmIdToNodeId` (low)

Locations consulted:
- declared & used: src/node.cpp:475-571

### Node::toJson::remappedId  [unsure / low / —]

Evidence:
- `node.cpp:506`: `int remappedId = idMap.at(asmId);`
- `node.cpp:550`: `{"nodeId", remappedId}` — emitted as the JSON
  `"nodeId"` value.

Interpretation:
- Local correctly describes *what was done* (remapped) but not what
  it semantically is (the JSON-side `nodeId`). Either name is
  defensible; the current is neutral, an alternative would be
  role-anchored.

Judgement:
- Slight readability question, not a misnomer.

Proposals:
- keep current (medium)
- `nodeIdJson` (low)

Locations consulted:
- src/node.cpp:506,550

### Node::fromJson::nId / aId / devIdx / cfg1 / cfg2  [unsure / low / —]

Evidence:
- `node.cpp:599-608`:
  ```cpp
  int nId    = static_cast<int>(obj.at("nodeId").as_int64());
  int aId    = static_cast<int>(obj.at("asmId").as_int64());
  int devIdx = static_cast<int>(obj.at("deviceIndex").as_int64());
  ...
  PlayConfig cfg1 = PlayConfig::fromJson(obj.at("config"));
  PlayConfig cfg2 = PlayConfig::fromJson(obj.at("currentCwvf"));
  ```

Interpretation:
- Each local is a parsed scalar destined for a same-named field in
  the constructor call (node.cpp:626-636). Abbreviated names
  (`nId`, `aId`, `devIdx`, `cfg1`, `cfg2`) save a few characters
  but hide the role; full-word names would be unambiguous and would
  match the field names that they are about to populate.
- `cfg1`/`cfg2` are particularly opaque: they correspond to the
  semantically distinct `config` and `currentCwvf` fields. The
  surrounding read-site uses the JSON keys directly so the
  separation is preserved, but downstream code (the ctor call) sees
  only `cfg1, cfg2` in argument-position 6/7.

Judgement:
- Local readability blemish; not catastrophic, but tightening the
  names would match the conventions used in `installPointers` (see
  `lookupNode`, `nextVal`, `loopVal`, `parentVal` — all role-anchored).

Proposals:
- `nodeIdJson`, `asmIdJson`, `deviceIndex`, `config`, `currentCwvf`
  (medium each)
- keep current (low)

Locations consulted:
- src/node.cpp:599-608, 626-636

## 4. Symbols inspected and judged routinely fine

- `Node::Node(simple)::type` — stored verbatim into `Node::type`.
- `Node::Node(simple)::asmId` — stored verbatim into `Node::asmId`.
- `Node::Node(full)::*` (all 20 params) — each has the same name as
  the field it initializes; this is the conventional "shadow-and-init"
  pattern and matches the JSON-anchored field names.
- `Node::type2str::t` — single param of switch; conventional.
- `Node::str2type::s` — string lookup key; conventional.
- `Node::last::node` — chain walker.
- `Node::insertBefore::newNode` — name describes role.
- `Node::insertBefore::par` — locked weak_ptr; abbreviation acceptable.
- `Node::updateParent::parent`, `::oldChild`, `::newChild` — names
  describe roles; matches header comments.
- `Node::updateParent::it` — vector iterator.
- `Node::remove::node`, `::par`, `::child` — obvious roles.
- `Node::swap::a`, `::b` — match the static method's signature; brief
  but contextual.
- `Node::swap::bNode`, `::aNode` — raw `Node*` aliases for shared_ptr
  args; unambiguous.
- `Node::swap::bParentCtrl`, `::bParentRaw`, `::aRaw`,
  `::parentLocked`, `::parentOfA`, `::locked`, `::current`, `::cur`,
  `::ancestor`, `::t`, `::bNext` — all describe the role they hold in
  the swap algorithm.
- `throwSwapNotConnected`, `throwSwapNotConnected::errStr`,
  `::formatted` — extracted helper for a duplicated `goto` target;
  names match content.
- `Node::clone::n` — single one-letter local for the new node.
- `Node::toJson::wavesFlat`, `::opt`, `::playIds`, `::wp`, `::sp`,
  `::branchIds`, `::child`, `::nextId`, `::loopId`, `::parentId`,
  `::lengthRegVal`, `::indexOffsetRegVal`, `::wavesArr`,
  `::branchArr`, `::playArr`, `::result` — all match their roles.
- `Node::fromJson::json`, `::obj`, `::wavesJsonArr`, `::waves`,
  `::elem`, `::typeStr`, `::typeStdStr`, `::nodeType`,
  `::lengthRegVal`, `::lReg`, `::lengthField`, `::idxOffsetRegVal`,
  `::ioReg`, `::tableIdx`, `::gRate`, `::precompFlags`, `::loopRuns`,
  `::branchSkips`, `::trigField`, `::node` — all match roles.
- `Node::installPointers::nodeMap`, `::json`, `::localMap`,
  `::lookupNode` (lambda), `::v`, `::id`, `::it`, `::obj`,
  `::playArr`, `::elem`, `::branchesArr`, `::nextVal`, `::loopVal`,
  `::parentVal`, `::oldNext`, `::oldLoop`, `::sp` — all match roles.

## 5. Coverage

**Fully covered:**
- `include/zhinst/node.hpp` — all in-scope symbols (NodeType enum and
  enumerators, all 22 declared `Node` data members, simple/full
  ctor parameter lists, free `operator==/!=/&/|` declarations).
- `src/node.cpp` — every in-scope local, parameter, lambda parameter,
  and namespace-scope free helper.

**Deferred:** none — single-pass scan covered all symbols.

**Not covered (out of scope per RULES §2/§3):**

- Type name `Node` (and the nested `NodeType` form
  `zhinst::Node::NodeType`). `nm --demangle _seqc_compiler.so`:
  - `00000000001cd860 t zhinst::Node::insertBefore(...)` etc.
  - `0000000000269970 t zhinst::Node::type2str(zhinst::Node::NodeType)`
  These confirm both `Node` and `NodeType` (as `Node::NodeType`) as
  binary-original type names; tier-1 excluded.
  *Note:* the source declares `NodeType` at namespace scope
  (`zhinst::NodeType`) rather than nested inside `Node`. The binary
  uses `zhinst::Node::NodeType`. This is a structural discrepancy
  worth recording in `incidental_findings.md`, but is not a renaming
  question (the name itself is `NodeType` either way).

- Method names — all present in `nm --demangle`:
  - `Node::Node(NodeType, int, int)` @ 0x12ace0
  - `Node::Node(int, int, vector<...>, ..., int)` @ 0x26c4a0
  - `Node::~Node()` @ 0x12afe0
  - `Node::type2str(NodeType)` @ 0x269970
  - `Node::str2type(string const&)` @ 0x26ac00
  - `Node::toString()` @ 0x264440
  - `Node::waveAtCurrentDeviceIndex() const` @ 0x1c7de0
  - `Node::last(shared_ptr<Node>)` @ 0x1d5cb0
  - `Node::insertBefore(shared_ptr<Node>)` @ 0x1cd860
  - `Node::updateParent(...)` @ 0x1d2f50
  - `Node::remove(shared_ptr<Node>)` @ 0x1d4440
  - `Node::swap(shared_ptr<Node> const&, shared_ptr<Node> const&)` @ 0x1d2720
  - `Node::clone() const` @ 0x1d5d40
  - `Node::toJson(unordered_map<int,int> const&) const` @ 0x264b90
  - `Node::fromJson(boost::json::value const&)` @ 0x268280
  - `Node::installPointers(...)` @ 0x269020

- Static data member `Node::idCounter_` — `nm` line
  `0000000000000044 b zhinst::Node::idCounter_`. Tier-1 excluded
  per RULES §3 (recorded in §3 above as a positive-evidence block
  for completeness).

- Default constructor `Node::Node()` — declared on the header
  (node.hpp:119) but **no symbol** in the binary
  (`grep "Node::NodeC1Ev\|Node::NodeC2Ev"` is empty). The header
  comment at node.cpp:25-37 explicitly notes the binary lacks
  `_ZN6zhinst4NodeC1Ev`. This is a reconstruction-side helper.
  Treated as in-scope (since the symbol isn't authoritative), but
  no parameters; only the existence of the function name itself
  could be flagged, and "Node()" is the only legal name for a
  default constructor.

- The `error_messages.hpp` enumerators `PrefetchError` and
  `SwapNotConnected` (used by `throwSwapNotConnected`) — owned by a
  different batch (`error_messages.hpp`). Not in scope here.

**Incidental observations** (recorded for `incidental_findings.md`,
not flagged as renames here):

1. `Node::NodeType` is nested in the binary mangling
   (`zhinst::Node::NodeType`) but is declared at namespace scope
   (`zhinst::NodeType`) in the reconstruction. Structural mismatch,
   not a name issue.
2. `Node::toJson` looks up `idMap.at(asmId)` and writes the result
   under JSON key `"nodeId"` (node.cpp:506,550), while the
   pointer-field id emissions use raw `Node::nodeId` (lines
   491,499,511,517,523). This may be a real semantic bug
   (inconsistent ID-space between the "nodeId" key and the
   reference-id keys) or evidence that `idMap` truly bridges
   asmId→nodeId. Worth verifying against the binary at @0x264fef.
3. `Node::Node()` (default ctor) has no symbol in the binary; it
   exists only because the reconstruction calls
   `make_shared<Node>()` somewhere — but `grep
   "make_shared<Node>()"` finds no callers in
   `reconstructed/src/`. Likely a dead reconstruction-side helper.
