# RULES — Symbol Renaming Audit

Authoritative description of how the **symbol renaming audit** is run.
While this audit is in progress, the standard working process described
in `AGENTS.md` (TODO-driven phases, OVERVIEW.md updates, source edits,
build verification, per-sub-phase commits) **does not apply**. See the
"Symbol renaming audits" section in `AGENTS.md` for the high-level
pointer; this file is the detailed and authoritative reference.

## 1. Purpose

A substantial fraction of identifiers in `reconstructed/` were named
when only partial information about the surrounding code was
available. Many of those names are still in place even after the
context became clearer. Misleading or non-descript names actively
harm debugging — they suggest meanings the code does not have. This
audit is a systematic, read-only sweep that identifies likely
misnomers and records renaming proposals as notes, **without
modifying any code**.

## 2. Scope

In scope:

- Parameters of free functions and of methods, **including default
  arguments**.
- Data members of structs and classes.
- Local variables, **only** when they are obviously misleading.
- **Namespace-scope named constants** (e.g. `kFoo`, `kBar`),
  including `inline constexpr` constants.
- **Enum members** (enumerators), e.g. `EnumType::Member`.
- **Project-defined type names** (struct/class/enum/typedef/`using`
  alias) — candidate for misnomer review, **subject to the
  symbol-table exclusion in §3**.
- **Macros** defined in headers under `reconstructed/include/zhinst/`.

Out of scope:

- Template parameters (`template <typename T>` etc.).
- Member type aliases (`using value_type = ...`).
- Anything in stdlib, third-party, generated code (`yy_*`, pybind
  glue, …).
- Symbols whose names are taken from the original binary, per §3.
- Any symbol whose declaration the auditor cannot locate (note in
  the report, do not invent).

### 2a. Type-suspicion as a side observation

Agents may record observations about a symbol's *type* if the
use-sites suggest a type mismatch (heavy casts, sentinel via special
value, unit confusion, narrowing/widening). Such observations belong
in the `Evidence` / `Interpretation` of an existing naming block. Do
**not** spawn separate type-only blocks: this audit is about names.

## 3. Authoritative exclusion: original-binary symbol table

The single authoritative source for what came from the original
binary is its mangled symbol table. The binary is at:

    /home/brian/zhinst/seqc_compiler/_seqc_compiler.so

Agents run `nm` / `objdump` directly when verification is needed.
Useful invocations:

    nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so
    objdump -tT --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so

What the symbol table **does** encode (Itanium ABI mangling):

- **Class/struct/enum type names** — authoritative. If a type appears
  in a mangled symbol there, the type name is excluded from rename.
- **Free function names** — authoritative; excluded.
- **Method names** (qualified by class) — authoritative; excluded.
- **Parameter types and qualifiers** — informational. Tells us
  *types*, **not** parameter names.

What it **does not** encode and so cannot exclude:

- **Parameter names.** Always in scope.
- **Data member names.** Always in scope.
- **Enum member names.** In scope. (RTTI strings or `.rodata` lookup
  tables — e.g. `EnumType::str()` mapping `0→"in"` — are *soft*
  evidence; useful but not decisive.)
- **Local variables.** In scope when misleading.
- **Macros.** In scope.
- **Most namespace-scope constants.** Only `extern` non-`inline`
  ones with external linkage appear in the table. Treat constants
  as in-scope unless `nm` shows the symbol exported.

**Static data members are tier-1 authoritative.** Itanium mangling
does encode the fully-qualified name of a `static` data member
(e.g. `_ZN6zhinst8Prefetch15minIndexedSizeE`). If `nm --demangle`
shows the static member as a data symbol, treat it as **excluded**
from rename, the same way a method or free-function name is.
Non-static (instance) data members are NEVER encoded and remain
in scope regardless of what `nm` shows for the enclosing class.

Source-code comments claiming "matches binary" or referencing
addresses are **soft** evidence only — they were often written
under the same uncertainty that produced suspect names, and remain
unverified until checked against the symbol table.

## 4. Method, not checklist

For every in-scope symbol the auditor must:

1. **Survey usage broadly and systematically.** Locate the
   declaration and all references — definitions, every call site
   for parameters, every read and write site for fields. At each
   site, read enough surrounding context to understand what is
   actually being done with the value. Do not stop at the first hit.
2. **Form a judgement** about whether the current name fairly
   describes that observed usage.
3. **Record the judgement with an explicit justification** that
   cites the concrete observations supporting it.

Confidence reflects how well the observations support the
judgement, not how many heuristic boxes were ticked.

### 4a. Examples of observations that often warrant a high-confidence flag

These are illustrative, **not exhaustive and not prioritized**.
Other context-specific observations may equally well justify a flag
— or, conversely, the items below may be misleading in context and
not warrant one. Use judgement.

- A comment near declaration or use site explicitly says the name
  is misleading, a placeholder, or uncertain.
- A comment describes a meaning that conflicts with what the name
  suggests.
- The name is generic or non-descript (`integer_1`, `arg2`,
  `field0`, `unk_*`, `tmp`, bare `data`/`value`/`x` outside math
  contexts) while the usage clearly has a specific role.
- The type or value range used at access sites contradicts the name
  (e.g. `count` only ever masked and bit-tested; `index` only
  compared to a sentinel).
- Unit/domain mismatch (bytes vs samples, ticks vs nanoseconds,
  zero-based vs one-based).
- All write sites assign a value whose provenance does not match
  what the name implies.
- A field is consistently passed into another function as a
  differently-named parameter, or vice versa — see §4c.

### 4b. Cautions

- **Comments are evidence, not proof.**
- **Do not over-fit to a single use site.**
- **Absence of a strong signal is not absence of a problem; presence
  of a signal is not proof of one.** Mark `unsure` / `low` rather
  than discard or overclaim.
- A flag is acceptable for reasons not enumerated above, **provided**
  the justification cites concrete usage observations.

### 4c. He-said/she-said arbitration

When a member `x_` is consistently passed as parameter `y` (or vice
versa), the inconsistency itself is the high-confidence finding.
Which side is wrong is a separate, often lower-confidence judgement.

- If the batch can decide which side is the misnomer, do so and
  record it normally.
- If it cannot, flag *both* sides (one in this batch, possibly the
  other in a different batch), list both names as candidate
  proposals (and "keep current name" if relevant), and use status
  `cross-batch-arbitration` with a `Cross-reference` to the
  counterpart's batch number.

### 4d. Positive-evidence blocks

Recording that a name **is** correct, with explicit evidence, is
allowed and useful — particularly when the symbol is involved in a
he-said/she-said situation (one side proven correct narrows the
other) or when readers later may be tempted to "fix" a correct
name. Use judgement = `no` and status = `not-misnomer`.

Tiers of positive evidence (strongest first):

1. **Type/method/free-function name** appears as such in the
   binary symbol table. Authoritative for that name.
2. **Member or enum name** appears verbatim as a JSON serializer
   key, ELF section name, log/error format string, or RTTI-style
   string in source. Strong, since these strings tend to be
   faithful from the binary's `.rodata`.
3. **Public API / node path** documented match.
4. Usage consistently and unambiguously matches the name across
   many sites with no contradicting context.

Routine "name fits usage" cases without recordable evidence
should stay in §4 of the report (bullets), not produce full
blocks.

## 5. Confidence

- **Confidence is in the correctness of the *judgement*** — not a
  priority, urgency, or severity.
- One supporting observation → **low**.
- Multiple independent observations all pointing the same way →
  **medium**.
- Authoritative external evidence (binary symbol table, faithful
  serializer key, RTTI string) → **high**.
- Each **proposal** also carries its own confidence. The more
  proposals there are, the lower the per-proposal confidence
  usually is. **Keeping the current name is a valid proposal.**

## 6. Status — closed vocabulary

Use exactly one of these or omit. Do not invent new values.

| Status | Meaning |
|---|---|
| (omitted = default) | Single-symbol finding, judgement stands; synthesis just considers proposals. |
| `verify-not-original` | Agent could not check the symbol table conclusively, or result was ambiguous. Synthesis re-checks. |
| `cross-batch-arbitration` | Counterpart symbol lives in another batch; arbitration deferred to synthesis. |
| `coordinated-rename` | Several in-batch symbols should be renamed together. |
| `not-misnomer` | Block records positive evidence the name is correct (see §4d). |

If none fits, describe the situation in `Judgement` and pick the
nearest. Do not coin new statuses.

## 7. Symbol-naming convention

Single canonical form per kind. Use this everywhere in the report.

- **Field**: `Class::field_`
- **Method**: `Class::method` (overload-disambiguated parenthetically
  when needed, e.g. `Class::allocate(5-arg)`)
- **Method parameter**: `Class::method::paramName`
- **Free-function parameter**: `funcName::paramName`
- **Local variable**: `Class::method::localName` (or `funcName::localName`)
- **Enum member**: `EnumType::Member`
- **Constant**: `ns::kName` (omit `ns::` if global)
- **Macro**: `MACRO_NAME`
- **Type**: `Class`

## 8. Mandatory report structure

Every report file under `reconstructed/notes/symbol-renaming-audit/`
follows this structure exactly:

```
# Batch NN — <name>

## 1. Files considered
## 2. Overview              ← table, fixed columns
## 3. Detailed findings     ← one subsection per non-routine row
## 4. Symbols inspected and judged routinely fine   ← bullets
## 5. Coverage              ← fully / deferred / out-of-scope
```

### 8a. §2 Overview table

Fixed columns:

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |

- `Misnomer?`: `yes` / `no` / `unsure`.
- `Conf`: `low` / `medium` / `high`.
- `Justification`: ≤ 5 words, telegraphic. (Detail goes in §3.)
- `Proposal(s)`: comma-separated; may be `—` if none. May include
  "keep current".
- `Status`: from the closed list in §6, or `—`.

Group rows by enclosing type, in this order within each group:
fields → methods (and per method: params → locals). Standalone
constants/enums/free-functions/macros each get their own group.

### 8b. §3 Detailed-finding subsection

One per row in §2 that warrants discussion. Routine `no` rows
(name matches usage, no recordable evidence either way) do **not**
need a subsection — they appear only in the table or in §4.
Subsection structure, in fixed order:

```
### <Symbol>  [yes|no|unsure / low|medium|high / status-or-—]

Evidence:
- file:line  exact snippet or quoted value
- ...

Interpretation:
- factual reading of what the evidence shows; no judgement

Judgement:
- single sentence answering "is this a misnomer?"

Proposals:                  (omit if judgement = no without rename intent)
- name1   (high|medium|low)
- name2   (low)
- keep current  (medium)    ← legitimate proposal

Cross-reference:            (only if status warrants)
- Counterpart Class::other_field_ in batch NN

Locations consulted:        (footer; terse; only what was actually read)
- declared: include/zhinst/foo.hpp:42
- used:     src/foo.cpp:100,138; src/bar.cpp:55
```

### 8c. Block discipline (the "court" rule)

- **Evidence** = cited facts. file:line + snippet/value. No
  interpretation.
- **Interpretation** = factual reading of what the evidence shows.
  No judgement.
- **Judgement** = single sentence answering one question: is the
  name a misnomer? `yes` / `no` / `unsure`. Must be supported by
  the interpretation above.
- Do not include observations in a block whose judgement they do
  not bear on. If a finding doesn't lead to a verdict on the
  block's symbol(s), it doesn't belong.
- Avoid prosecutorial speeches about why something was *not*
  flagged ("this is only a readability concern, not flagged" — if
  you don't flag it, don't write a block; mention it in §4 if
  worth a line).

### 8d. §4 Routine-fine list

One bullet per symbol, brief reason (≤ 1 short sentence).
Distinguishes "looked at and OK" from "not looked at".

### 8e. §5 Coverage

- **Fully covered:** what.
- **Deferred:** what and why.
- **Not covered (out of scope per RULES §2/§3):** what and why
  (e.g. method names found in symbol table).

## 9. Workflow phases

- **A. Inventory.** Coordinator pass enumerates classes/free
  functions and writes the README assignment index. Read-only.
- **B. Scan.** Subagent batches, one per assignment. Each writes
  its report directly into its assigned file in this folder.
  Coordinator updates README status as reports land.
- **C. Synthesis.** Separate, user-driven step. Merges per-batch
  reports into a single ranked proposal list. **Not** automatic —
  scan stops and returns to the user for review first.
- **D. Execution.** Renames are a future task, queued through
  `TODO.md` after synthesis. **Not** part of this audit.

## 10. Batch granularity and subagent discipline

- One class (`.hpp` + `.cpp` pair) per subagent assignment is the
  default. Free functions/headers without a class are grouped by
  file or topical cluster.
- Subagent prompts must:
  - Restrict the agent to the assigned symbols.
  - State the audit scope (§2), the exclusion rule (§3), the
    method (§4), the status vocabulary (§6), the symbol naming
    convention (§7), and the mandatory report structure (§8) by
    reference to this file.
  - Tell the agent that **partial reports are preferred over
    rushed ones**: if context is filling, write what is solid
    and stop, marking what is uncovered in §5.
  - Forbid speculation beyond what observations support.
  - Direct the agent to write its report directly into its
    assigned file under
    `reconstructed/notes/symbol-renaming-audit/`.

## 11. No edits outside the audit folder

For the duration of this audit, **no file outside
`reconstructed/notes/symbol-renaming-audit/` may be edited**.
This includes source files, headers, `TODO.md`, `OVERVIEW.md`,
other notes files, and build files.

`RULES-symbol-renaming.md` itself and the AGENTS.md addendum
that points to it are written *once* at the start of the audit
and from then on also fall under this rule. Actionable items
produced by the audit are queued inside the audit folder and only
later promoted to `TODO.md` after synthesis and user review.

## 12. No commits during scan

Do not create git commits during the scan phase. Commits, if any,
are made at synthesis time under user direction.
