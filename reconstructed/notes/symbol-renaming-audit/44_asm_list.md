# Batch 44 — asm_list

## 1. Files considered

- `reconstructed/include/zhinst/asm_list.hpp`
- `reconstructed/src/asm_list.cpp`

Symbol-table reference (anchor for §3 exclusions):

- `nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`
  shows the following authoritative names (excluded from rename):
    - Type: `zhinst::AsmList`, nested type `zhinst::AsmList::Asm`
    - Methods:
      `AsmList::~AsmList` (0x11d5b0),
      `AsmList::append(Asm const&)` (0x15d180),
      `AsmList::print(bool, ostream&, bool) const` (0x264250),
      `AsmList::serialize() const` (0x2646d0),
      `AsmList::deserialize(string const&)` (0x266050),
      `AsmList::parseStringToAsmList(string const&)` (0x266160),
      `AsmList::maxRegister() const` (0x269910),
      `AsmList::Asm::~Asm` (0x122dd0),
      `AsmList::Asm::serializeNodeToJsonString(unordered_map<int,int> const&) const` (0x2698b0),
      `AsmList::Asm::createUniqueID(bool)` (always inlined; mangled
      static `nextID` proves the symbol exists).
    - Function-static: `AsmList::Asm::createUniqueID(bool)::nextID`
      at `0x40 b` — **tier-1 authoritative**, excluded from rename.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AsmList::Asm::sequenceId` (field) | no | high | matches `nextID` & `Node::asmId` | keep current (high) | not-misnomer |
| `AsmList::Asm::assembler` (field) | no | high | type is `AssemblerInstr` | keep current (high) | — |
| `AsmList::Asm::wavetableFront` (field) | unsure | medium | dual-purpose; doc-only line-num alias | keep current; rename to `wavetableFrontIndex_` consistency; rename to neutral `slot0x88` | cross-batch-arbitration |
| `AsmList::Asm::node` (field) | no | high | shared_ptr<Node>; matches usage | keep current (high) | — |
| `AsmList::Asm::isWaveformCmd` (field) | yes | high | semantic inversion (control opcodes) | rename per batch 26 (`noOpt`/`skipOptimization`/`isControlOpcode`) | cross-batch-arbitration |
| `AsmList::Asm::lineNumber()` (method/alias) | unsure | medium | accessor for dual-use field | keep; rename if field renames | cross-batch-arbitration |
| `AsmList::Asm::createUniqueID::reset` (param) | no | high | controls reset vs increment | keep current (high) | — |
| `AsmList::Asm::serializeNodeToJsonString::idMap` (param) | no | high | passed to `Node::toJson(idMap)` | keep current (high) | — |
| `AsmList::Asm::operator==::other` (param) | no | medium | conventional name | keep current (high) | — |
| `AsmList::append::entry` (param) | no | high | descriptive, matches role | keep current (high) | — |
| `AsmList::print::showNode` (param) | no | high | gates "// placeholder:" branch | keep current (high) | — |
| `AsmList::print::os` (param) | no | high | output stream sink | keep current (high) | — |
| `AsmList::print::showHeader` (param) | no | high | gates "NNN (NNN): " line prefix | keep current (high) | — |
| `AsmList::deserialize::str` (param) | no | medium | parsed assembly text | keep current (high) | — |
| `AsmList::parseStringToAsmList::str` (param) | no | medium | parsed assembly text | keep current (high) | — |
| `AsmList::insert(shared_ptr<Node>, AsmList&)::placeholder` (param) | no | medium | matches `placeholder` Node usage | keep current (high) | — |
| `AsmList::insert(...)::source` (param) | no | medium | source range to splice | keep current (high) | — |
| `AsmList::insert(... iterator)::first/last` (params) | no | high | std iterator naming | keep current (high) | — |
| `AsmList::AsmList(Asm)::single` (param) | no | low | single-element conversion | keep current (medium) | — |
| `AsmList::operator=(Asm)::single` (param) | no | low | single-element conversion | keep current (medium) | — |
| `AsmList::operator=(vector<Asm>)::v` (param) | unsure | low | bare letter | `entries`; keep | — |
| `AsmList::operator==::other` (param) | no | high | conventional | keep current (high) | — |
| `swap::a, b` (free fn params) | no | high | conventional swap params | keep current (high) | — |
| `AsmList::entries` (data member) | no | high | obvious | keep current (high) | — |
| `nextSequenceId()` (free fn name) | no | high | thin wrapper, accurate | keep current (high) | not-misnomer |
| `AsmList::maxRegister::maxReg/result/valid/regNum` (locals) | no | medium | descriptive | keep current (high) | — |
| `AsmList::serialize::idMap` (local) | no | high | matches Node::toJson API | keep current (high) | — |
| `AsmList::serialize::nextIdx` (local) | no | high | sequential counter | keep current (high) | — |
| `AsmList::parseStringToAsmList::wavetableFront` (local) | unsure | medium | mirrors field's dual-purpose name | keep; rename with field | cross-batch-arbitration |
| `AsmList::parseStringToAsmList::imm1`,`imm2` (locals) | yes | low | unused / dead-init | drop; rename to descriptive | — |
| `AsmList::parseStringToAsmList::vec_input/vec_reg/vec_output` (locals) | no | medium | descriptive of categorisation | keep current (medium) | — |
| `AsmList::parseStringToAsmList::regOrder` (local) | no | high | matches `getRegisterOrder()` return | keep current (high) | — |
| `AsmList::parseStringToAsmList::idMap`,`nodeMap` (locals) | no | high | maps used for noOpt path | keep current (high) | — |

## 3. Detailed findings

### `AsmList::Asm::sequenceId`  [no / high / not-misnomer]

Evidence:
- `asm_list.cpp:35-41`: `createUniqueID(false)` returns
  `nextID++` (TLS thread-local). Each `Asm` is stamped with the
  next id.
- `asm_list.cpp:533-541`, `:537`:
  `int seqId = Asm::createUniqueID(false); ...
  entry.sequenceId = seqId;` — direct write site.
- `asm_commands.cpp:55,66,76,184,203` and many sites in
  `asm_commands_impl_*.cpp`: `result.sequenceId = nextSequenceId();`.
- `prefetch_emit.cpp:90,100`: `findPlaceholder` matches by
  `entry.sequenceId == targetId` where `targetId = node->asmId`.
- `notes/symbol-renaming-audit/20_node.md:39`: `Node::asmId`
  documented as `result.sequenceId` source — bidirectional
  consistency.
- `compiler.cpp:323`: `Node(NodeType::Entry, placeholderAsm.sequenceId, …)`.

Interpretation:
- The field stores a per-Asm unique sequence id allocated from a
  monotonically increasing counter; it is the linkage key between
  asm entries and the `Node` graph.

Judgement:
- No — the name `sequenceId` accurately describes the value; the
  counter origin (`createUniqueID`/`nextID`) and downstream key
  use (`Node::asmId`) both agree.

Locations consulted:
- declared: `include/zhinst/asm_list.hpp:55`
- used: `src/asm_list.cpp:35-41,133,187,353,398,417,541,598`;
  `src/asm_commands*.cpp` (many); `src/prefetch_emit.cpp:90,100`;
  `src/compiler.cpp:323`.

---

### `AsmList::Asm::wavetableFront`  [unsure / medium / cross-batch-arbitration]

Evidence:
- `asm_list.hpp:58-72`: header explicitly documents the field as
  dual-purpose: for normal commands it stores
  `AsmCommands::wavetableFrontIndex_`; for MESSAGE / ERROR_MSG it
  is read as a source line number, exposed via
  `lineNumber()` accessor methods.
- `asm_commands.cpp:42`: `wavetableFrontIndex_(0)` initialiser;
  `asm_commands.cpp:57,68,78,186,205,817,832`:
  `result.wavetableFront = wavetableFrontIndex_;` etc.
- `prefetch_placesingle.cpp:78`:
  `asmCommands_->setWavetableFrontIndex(placeholder->wavetableFront);` —
  the field is round-tripped back into `wavetableFrontIndex_`.
- `compiler.cpp:640`: `result.push_back(entry.lineNumber());`
  iterates entries and reads the same +0x88 slot as a line number.
- `asm_optimize.cpp:653,669`:
  `int lineNr = it->lineNumber(); // +0x88 (alias for wavetableFront on MESSAGE/ERROR_MSG)`.
- `notes/symbol-renaming-audit/25_asm_optimize.md:85`:
  treats the field as `keep current (high)` for the
  `splitConstRegisters` barrier-clone path; that decision rests on
  the field correctly being the wavetable front index there, not
  on the field name being globally good.
- `notes/symbol-renaming-audit/49_asm_commands_impl.md:39-65`:
  multiple `lineNumber` parameters in `wvf*` / `brz` / `ssl` / `ssr`
  / `ldiotrig` are all "passed straight to wavetableFront" — i.e.
  the consumer side uses both vocabularies for the same int.

Interpretation:
- The slot is genuinely dual-purpose. The name `wavetableFront`
  correctly describes one of the two uses (and it is the dominant
  one). The header acknowledges the second use by providing
  `lineNumber()` accessors rather than splitting the layout.
- The dual purpose itself is a structural property of the binary
  (single 4-byte slot at +0x88); a rename to a neutral name
  (e.g. `slot0x88`, `auxIndex`) would not improve clarity, and
  renaming to `wavetableFrontIndex_` to match
  `AsmCommands::wavetableFrontIndex_` would harm the line-number
  use sites.

Judgement:
- Unsure — the name is faithful to the primary use and the
  secondary use is mediated by the `lineNumber()` accessor. There
  is no clearly better single name.

Proposals:
- keep current  (medium)
- rename to `wavetableFrontIndex_` (low) — would unify with the
  setter/getter on `AsmCommands` but degrades the line-number sites.
- rename to neutral `auxIndex` / `slotA88` (low) — would force every
  call site to read context.

Cross-reference:
- Counterpart `AsmCommands::wavetableFrontIndex_` evaluated in
  batch 10 (`keep current (high)`); `wvf*` / `brz` `lineNumber`
  parameters in batch 49 (all `keep current (high)`).

Locations consulted:
- declared: `include/zhinst/asm_list.hpp:58`
- used: `src/asm_list.cpp:131,355,400,419,543`;
  `src/asm_commands.cpp:57,68,78,186,205,817,832,875,883`;
  `src/prefetch_placesingle.cpp:76-78`;
  `src/asm_optimize.cpp:1197,1210,1217,1221,1222`;
  `src/compiler.cpp:640`.

---

### `AsmList::Asm::lineNumber()`  [unsure / medium / cross-batch-arbitration]

Evidence:
- `asm_list.hpp:71-72`:
  `int& lineNumber()       { return wavetableFront; }` (and const
  overload). Comments state: "For MESSAGE / ERROR_MSG instructions
  it holds a source line number".
- `asm_optimize.cpp:653`:
  `int lineNr = it->lineNumber(); // +0x88 (alias for wavetableFront on MESSAGE/ERROR_MSG)`.
- `compiler.cpp:640`: `entry.lineNumber()` iterated to build a
  separate line-number table.
- Free function `nextSequenceId()` and accessor `lineNumber()`
  together comprise an alias-method cluster (per batch 24's
  `lineNumber()`/`labelType()` pattern on `AsmExpression`).

Interpretation:
- `lineNumber()` is a typed alias for the dual-purpose +0x88 slot
  used only on MESSAGE/ERROR_MSG/opcode-4 entries. The accessor
  hides the dual-use from those sites; the name is correct for
  the role those callers care about.
- Whether the alias-vs-field naming choice is the right shape is
  conditional on how the field is renamed (if at all).

Judgement:
- Unsure — the accessor name is locally faithful, but its existence
  pre-supposes the field name `wavetableFront` is also acceptable.

Proposals:
- keep current  (high)
- (track with `wavetableFront` rename outcome) (low)

Cross-reference:
- Field `AsmList::Asm::wavetableFront` in this batch (above).
- Alias-method cluster mirrors batch 24's `lineNumber()`/`labelType()`
  on `AsmExpression`.

Locations consulted:
- declared: `include/zhinst/asm_list.hpp:71-72`
- used: `src/asm_optimize.cpp:653,669`; `src/compiler.cpp:640`.

---

### `AsmList::Asm::isWaveformCmd`  [yes / high / cross-batch-arbitration]

Evidence:
- `asm_list.hpp:61`: `bool isWaveformCmd = false;` — field declared
  here (per batch 25 finding noted in the brief, the field is
  *owned* in this batch).
- `asm_list.cpp:357,402,421,575`: written from
  `entry.isWaveformCmd = isWaveformCmd(instr);` where the free
  predicate `isWaveformCmd(instr)` (assembler.hpp:202-205) is
  defined as `(static_cast<uint32_t>(instr.cmd) - 3u) < 3u`,
  i.e. true iff `cmd ∈ {3, 4, 5}` = `{MESSAGE, opcode-4, ERROR_MSG}`
  — explicitly the **non-waveform** control/message pseudo-opcodes.
- `asm_list.cpp:200`:
  `if (entry.isWaveformCmd && opcode != 3 && opcode != 4 && opcode != 5)`
  — the field is true exactly for {3,4,5}, so the conjunction is
  always false; this serialise-side branch never fires under the
  normal-write path. The field is thus used as a "this-is-a-
  control-pseudo" flag, not a waveform flag.
- `asm_list.cpp:569,573`: in the parser, the noOpt JSON path and
  the `isWaveformCmdOverride_` path both force
  `entry.isWaveformCmd = true` regardless of `cmd` — a *override
  path*, again not a waveform predicate.
- `asm_optimize.cpp:343,454`: `if (it->isWaveformCmd) continue;` —
  used as a skip-optimization gate (per batch 25 finding).
- `asm_optimize.cpp:1197,1210,1222`:
  `barrier.isWaveformCmd = false; ... orig.isWaveformCmd = (cmd-3) < 3u;`
  — barrier clone path explicitly recomputes from cmd value.
- `asm_commands.cpp:188,207`,
  `asm_commands_impl_hirzel.cpp:123`:
  `result.isWaveformCmd = flag;` (per batch 10 / 49 findings).
- Cross-batch records: batch 26 (free fn `isWaveformCmd` flagged
  yes/high), batch 25 (this field flagged yes/high), batch 24
  (`isWaveformCmdOverride_` field flagged not-misnomer because it
  matches the asm_list field name).

Interpretation:
- The field name asserts "this Asm is a waveform command", but the
  actual write-side semantics evaluate to true for the
  control/message pseudo-opcodes (3,4,5) **plus** an override path
  that means "do not optimise this entry". The read-side use in
  `asm_optimize` treats it as "skip optimisation", and the
  serialise-side `#disableOpt` suffix logic confirms that reading.
- All three sibling names — the free predicate, this field, and
  the `isWaveformCmdOverride_` AsmExpression bit — share the same
  semantic inversion. They must be renamed together.

Judgement:
- Yes — the field name is semantically inverted relative to the
  values written to it and the optimisation-suppression role it
  plays at read sites.

Proposals:
- `noOpt`  (high)
- `skipOptimization`  (medium)
- `isControlOpcode`  (medium) — accurate for the cmd ∈ {3,4,5}
  write path but loses the override-path meaning.
- keep current  (low)

Cross-reference:
- Free fn `isWaveformCmd` and param `instr` — batch 26.
- Use-sites — batch 25 (`asm_optimize`), batch 10 (`asm_commands`),
  batch 49 (`asm_commands_impl`); the latter two have `flag`→
  `isWaveformCmd` proposals that must be re-evaluated together.
- `AsmExpression::isWaveformCmdOverride_` — batch 24
  (`not-misnomer` solely because it matches *this* field; if the
  field renames, that finding must be revisited).

Locations consulted:
- declared: `include/zhinst/asm_list.hpp:61`
- used: `src/asm_list.cpp:200,357,402,421,569,573,575`;
  `src/asm_optimize.cpp:343,454,1197,1210,1222`;
  `src/asm_commands.cpp:59,70,80,188,207,412,418,819,834`;
  `src/asm_commands_impl_hirzel.cpp:123`;
  `src/asm_commands_impl_cervino.cpp:109`.

---

### `nextSequenceId()` (free function name)  [no / high / not-misnomer]

Evidence:
- `asm_list.hpp:211`:
  `inline int nextSequenceId() { return AsmList::Asm::createUniqueID(false); }`
  — thin alias.
- `nm` shows `AsmList::Asm::createUniqueID(bool)::nextID` as the
  TLS counter; the field stamped is `Asm::sequenceId`. Function
  name accurately describes what it returns.
- 23 call sites in `asm_commands*.cpp` of the form
  `result.sequenceId = nextSequenceId();` — the bidirectional
  consistency confirms the name fits the role.
- `nextSequenceId` itself does NOT appear in the binary symbol
  table (per `nm | grep nextSequenceId` — only the underlying
  static `nextID`). It is therefore in scope per §3.

Interpretation:
- A header-only inline alias for `Asm::createUniqueID(false)` whose
  return value is always assigned to `Asm::sequenceId`; the name
  is accurate and the only use sites uniformly support it.

Judgement:
- No — name correctly describes the action.

Locations consulted:
- declared: `include/zhinst/asm_list.hpp:211`
- used: `src/asm_commands.cpp:55,66,76,184,203`;
  `src/asm_commands_impl_hirzel.cpp:23,34,46,79,97,111,131,144,157`;
  `src/asm_commands_impl_cervino.cpp:30,42,74,104,117,130,143`.

---

### `AsmList::parseStringToAsmList::imm1`,`imm2` (locals)  [yes / low / —]

Evidence:
- `asm_list.cpp:309-310`:
  ```
  Immediate imm1;  // initialized with float 1.0f (0x3f800000)
  Immediate imm2;  // initialized with float 1.0f (0x3f800000)
  ```
- No reads of `imm1`/`imm2` anywhere in the function body — they
  appear to be reconstruction vestiges of stack-allocated
  scratch slots from disassembly that were never wired up.

Interpretation:
- These appear to be dead initialisers in the reconstruction
  rather than live values feeding the algorithm.

Judgement:
- Yes — names are bare-numeric stand-ins for what may be unused
  storage; if reconstruction work later proves they are live, a
  semantic name is needed.

Proposals:
- drop (high) — if confirmed unused
- rename to descriptive name once role is understood (low)
- keep current  (low)

Locations consulted:
- declared: `src/asm_list.cpp:309-310`

## 4. Symbols inspected and judged routinely fine

- `AsmList::Asm::assembler` — type `AssemblerInstr`, sole nested
  command record; trivially correct.
- `AsmList::Asm::node` — `shared_ptr<Node>`; trivially correct,
  matches `node->toJson(...)` and `node->toString()` use sites.
- `AsmList::Asm::createUniqueID::reset` — bool gating
  reset-to-zero vs post-increment; correctly named.
- `AsmList::Asm::serializeNodeToJsonString::idMap` — passed
  verbatim into `Node::toJson(idMap)` (matching `Node::toJson`
  signature); correctly named.
- `AsmList::Asm::operator==::other` — conventional.
- `AsmList::append::entry` — descriptive of an `Asm` argument.
- `AsmList::print::showNode`, `os`, `showHeader` — each
  unambiguously gates a print branch / receives output.
- `AsmList::deserialize::str`, `parseStringToAsmList::str` — both
  receive the assembly text string.
- `AsmList::insert(shared_ptr<Node>, AsmList&)::placeholder` /
  `::source` and the iterator overload's `first`/`last` — header
  overloads (the conversion-via-`shared_ptr<Node>`) are not called
  from any site under `src/` (call sites use the
  `insert(iterator, first, last)` std-vector overload directly).
  Names are consistent with `Prefetch::placeSingleCommand` doc.
- `AsmList::AsmList(Asm)::single`, `operator=(Asm)::single` —
  conversion constructors; `single` reads naturally as
  "a single Asm".
- `AsmList::operator=(vector<Asm>)::v` — bare-letter; only used
  inside `entries = std::move(v);`. Marginal; keeping.
- `swap::a`, `swap::b` — std-style.
- `AsmList::entries` — sole storage member; trivial.
- `AsmList::maxRegister::maxReg`, `result`, `valid`, `regNum`
  — descriptive locals consistent with their roles.
- `AsmList::serialize::idMap`, `nextIdx`, `opcode`, `entry`, `ss`,
  `jsonVal` — all descriptive.
- `AsmList::parseStringToAsmList::constants`, `assembler`,
  `expressions`, `result`, `wavetableFront` (local mirrors the
  field name; tracked in §3 with the field), `vec_input`,
  `vec_reg`, `vec_output`, `expr`, `instr`, `seqId`,
  `regOrder`, `regExprs`, `cmdVal`, `exprCmd`, `imm`, `child`,
  `inputExpr`, `outExpr`, `labelStr`, `commentSv`, `jv`, `node`,
  `nodeSeqId`, `report`, `nodeMap`, `idMap`, `parsed`,
  `warnings` — all carry their role; nothing flagged.

## 5. Coverage

- **Fully covered:** all in-scope symbols in
  `include/zhinst/asm_list.hpp` and `src/asm_list.cpp` (data
  members, free function `nextSequenceId`, parameters of all
  declared methods including default args, locals in
  `maxRegister`, `print`, `serialize`, `deserialize`, and
  `parseStringToAsmList`, plus the alias-method
  `lineNumber()`).
- **Deferred:** none from this batch alone — all unsure/yes
  verdicts are routed through cross-batch arbitration with
  batches 26, 25, 10, 49, 24, 03, 20.
- **Not covered (out of scope per RULES §2/§3):**
    - Type names `AsmList`, `AsmList::Asm` — present in mangled
      symbols; excluded by §3 tier-1.
    - Method names `~AsmList`, `~Asm`, `append`, `print`,
      `serialize`, `deserialize`, `parseStringToAsmList`,
      `maxRegister`, `serializeNodeToJsonString` — present in
      symbol table; excluded by §3.
    - `createUniqueID`, including its `nextID` static —
      authoritative per §3 (function-local static is mangled and
      visible in `nm`).
    - `using iterator = std::vector<Asm>::iterator;` and
      `const_iterator` — member type aliases, excluded per §2.
    - Template parameter `Args...` on the variadic
      `insert(...)` overload — excluded per §2.
