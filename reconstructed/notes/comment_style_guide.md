# Comment Style Guide — reconstructed/

This document is the authoritative reference for comment conventions across all
source files under `reconstructed/src/` and `reconstructed/include/zhinst/`.

The goal is a consistent, reader-focused set of annotations that permanently
link every reconstructed symbol to its binary origin, without carrying
reconstruction-process artifacts (phase IDs, correction notices, GDB provenance
tags) that have no long-term technical value.

---

## 1. File-level banner

Every `.cpp` and `.hpp` file begins with exactly this block:

```cpp
// ============================================================================
// <ClassName or module name> — <one-line description>
// Reconstructed from _seqc_compiler.so
// ============================================================================
```

- One line of description. No phase IDs, no sub-phase references, no method
  enumeration, no `sizeof` here.
- Headers that define a class may add a `// sizeof = 0xNN` line immediately
  after the banner (before the `#pragma once` / include guards), but the
  detailed layout lives with the struct declaration (see §4).

---

## 2. Function address (mandatory)

Every reconstructed function definition is preceded by its binary offset on its
own line:

```cpp
// 0x1c5850
Prefetch::Prefetch(AWGCompilerConfig const& config, ...) {
```

A range is used when the full extent is known and useful:

```cpp
// 0x1c5850..0x1c5a53
```

- No prose on this line.
- Constructors with a separate exception-cleanup tail may note it:
  `// 0x1c5850..0x1c5a53  (cleanup 0x1c5a54..0x1c5b59)`

---

## 3. Inline instruction annotation

Pins a source line to a binary instruction address:

```cpp
ctx.messages->setLineNr(lineNr);   // @0x20c6d3
if (x == Assembler::INVALID)       // @0x1213a2: skip dead instructions
```

Raw x86-64 disassembly is included **only when it justifies non-obvious code
shape** — a surprising register choice, an inlined call, a non-obvious field
access. Drop it when the source line already makes the behavior clear.

```cpp
// lea rdi,[r15+0x20]; test cl,bl  — cmdType & 1 check
```

---

## 4. Struct / class layout annotation

Offsets are interleaved with member declarations. The `vptr` is always stated
explicitly for polymorphic types. Padding is shown as a commented-out member
only when the gap is ≥ 4 bytes. The `sizeof` verification closes the block.

```cpp
// Vtable @ 0xb04e38
class Foo {
    // vptr              // +0x00
    T*    foo_;          // +0x08
    Bar   bar_;          // +0x10  (0x18 bytes)
    bool  flag_;         // +0x28
    // char[7] __pad;    // +0x29
    // sizeof = 0x30  (verified @ 0x285050)

    virtual void evaluate();        // vtable[0] +0x00
    virtual void print();           // vtable[1] +0x08
    virtual ~Foo();                 // vtable[2] +0x10 (complete)
                                    // vtable[3] +0x18 (deleting)
};
```

For plain structs with no vtable the `// Vtable` line and virtual annotations
are omitted.

ABI notes (libc++ vs libstdc++ size differences) belong immediately after the
`sizeof` line:

```cpp
    // sizeof = 0x28  (verified @ 0x285050)
    // NOTE: libc++ unordered_map is 0x28; libstdc++ is 0x38.
```

---

## 5. Binary-behavior remarks

Documents intentional deviations from what idiomatic C++ would do, ABI
substitutions, or non-obvious invariants observed in the binary:

```cpp
// Binary: does NOT validate same-length inputs — silently accepts mismatched sizes.
// Binary: boost::filesystem::ofstream; std::ofstream is ABI-equivalent here.
// Binary: Phase 5 is unconditional — runs for both single- and multi-value paths.
```

Single-line form preferred. Use a block comment for longer explanations.
This replaces the former `NOTE:` / `IMPORTANT:` / `GDB-verified:` prefix
variants that documented binary behavior.

---

## 6. Not-in-binary remarks

```cpp
// Not in binary — helper introduced for readability.
// Inlined at all call sites in binary.
```

---

## 7. `.rodata` constant references

```cpp
amplitude = 1.0;   // rodata @ 0x956030
```

---

## 8. `// sic` — verbatim binary strings

Marks a seemingly incorrect spelling or value that exactly matches the binary:

```cpp
return "unknnown";   // sic — matches binary string literal
```

---

## 9. Algorithm outline in large functions

Where a prose outline already exists, it uses numbered steps immediately after
the opening brace, with matching `// ---` section dividers inside the body:

```cpp
void Compiler::compile(...) {
    // 1. Reset messages and set up callbacks.
    // 2. Normalize line endings.
    // 3. Parse → shared_ptr<Expression>.
    // 4. Evaluate AST.
    // 5. Run prefetcher.

    // --- 1. Reset messages ---
    ...
    // --- 2. Normalize ---
    ...
}
```

Do not create outlines where none exist. Where phase or path language in the
body makes a prose outline clearly warranted but none is present, leave:

```cpp
// TODO: add prose overview for this function.
```

---

## 10. Branch labels

Used to label top-level control-flow forks that correspond to distinct binary
code paths:

```cpp
// Branch A: known user-defined function   (@0x20c7bd)
...
// Branch B: delegate to CustomFunctions   (@0x20c800)
```

---

## 11. `TODO` markers

Only for genuinely incomplete reconstruction. Must cite a binary address:

```cpp
// TODO: reconstruct cervino Load prf path (binary @ 0x1d1a84)
```

---

## 12. Incidental-finding cross-references (`IF-NNN`)

Always prefixed with `NOTE:` (finding is informational, no action required) or
`TODO:` (finding still needs follow-up work):

```cpp
// NOTE: IF-143 — binary gates this on split_=false (checks 0xbc(%r14) @ 0x1d1a84).
// TODO: IF-105 — verify full sequence; see incidental_findings.md.
```

---

## 13. Documentation comments (`//!` and `/*! */`)

Documentation comments are **distinct from reconstruction-annotation
comments**.  They use Doxygen-compatible markers so that
`reconstructed/docs/` can render them as a browsable site.  See
`reconstructed/docs/Doxyfile.in` for the full configuration.

- Single-line: `//!`
- Multi-line block: `/*! ... */`

The plain `//` form is **reserved for reconstruction notes** (addresses,
`// Binary:`, layout offsets, etc., per §1–§12).  Never mix the two
markers in a single comment block.

### Project-specific aliases

Three custom Doxygen aliases carry the project's accuracy discipline.
Each generates an entry on its own aggregated cross-reference page so
the entire backlog is discoverable from the rendered site.

| Alias | Meaning |
|---|---|
| `\unclear`    | Behaviour or semantics genuinely unknown; needs investigation. |
| `\verifyme`   | Believed correct from disasm/notes but not yet GDB- or test-verified. |
| `\binarynote` | Verified fact about the binary that diverges from idiomatic C++ (informational, not a gap). |

Three further provenance aliases keep claims traceable:

| Alias | Meaning |
|---|---|
| `\reconstructed{ADDR}` | Symbol reconstructed from binary @ADDR. |
| `\seenotes{file.md}`   | Cross-reference to a `reconstructed/notes/` topic. |
| `\verified{tag}`       | Provenance tag, e.g. `\verified gdb` / `\verified test:foo`. |

### Worked example

```cpp
//! \brief Computes the next available cache-line slot for a waveform.
//!
//! Walks `pointers_` looking for a contiguous gap of at least
//! `requestedSize` bytes starting on a 64-byte boundary.
//!
//! \reconstructed{0x2aa960}
//! \seenotes{memory_allocator.md}
//!
//! \param requestedSize Allocation size in bytes; must be a multiple of 4.
//! \return Byte offset of the first free slot, or -1 if none fits.
//!
//! \verifyme — the alignment requirement is inferred from the binary's
//! `and 0xfffffffc` mask but no test exercises non-aligned input yet.
int MemoryAllocator::findFreeSlot(uint32_t requestedSize);
```

### Accuracy rules

1. Every claim must be backed by disassembly evidence, GDB verification,
   a notes-file cross-reference, or test coverage.
2. When none apply, use `\unclear` or `\verifyme` — never guess.
3. Documentation comments are not a substitute for the reconstruction
   notes (binary addresses, layout, etc.).  They complement them.

---

## 14. Coexistence of `//!` documentation and `//` reconstruction notes

When adding `//!` documentation to a declaration that already carries
reconstruction annotations, the rule is **content preservation**, not
form preservation.  Reconstruction notes carry information the docs
site deliberately does not surface — binary addresses, struct field
offsets, ABI padding, "reconstructed from 0x…" provenance, observed
behaviour, JSON-key inventories, cross-references to `notes/`.  That
information must survive every documentation pass.  The form it lives
in (line, position, surrounding wording) is fluid; the **facts** are
not.

### What this means in practice

1. **Information first.**  Every fact in a `//` comment — binary
   address, field offset, padding marker, behavioural observation,
   "reconstructed from", note cross-reference — must still be present
   somewhere in the file after a `//!` pass.  It can move (line,
   position, wording), but it cannot be silently dropped.

2. **Verify-then-write still applies (per AGENTS.md).**  When
   integrating recon notes alongside `//!`, double-check the claim
   against the disassembly / function body.  If a `//` claim is wrong
   (drifted offset, stale address after a renaming pass, contradictory
   semantics) — **correct it**, log an IF for the discrepancy, and
   keep the corrected fact.  The `//` text is not immutable; only the
   information content is non-negotiable.

3. **Two audiences, two channels.**  `//!` is for the user of the docs
   site (behaviour, contract, types, units).  `//` carries
   recon-internal facts that we deliberately do not surface in the
   docs (offsets, addresses, ABI footnotes, internal-only provenance).
   When a fact is genuinely both — e.g. a behavioural observation with
   no binary-specific anchoring — folding it into the `//!` brief and
   removing the `//` original is fine, because the information has
   moved, not vanished.

4. **Cosmetic-only `//` lines** carry no information and may be
   replaced.  The test is: *does this `//` line carry a fact that
   isn't visible elsewhere in the file?*  Examples of cosmetic-only:
   - `// --- Static shift constants ---` section dividers (replace
     with `\name` group when desired).
   - Heading-style separators with no content beyond the heading.

5. **Anti-patterns** (observed in D5-1 and corrected):
   - Deleting `// Reconstructed from operator!= at 0x1d5770.` — this
     is recon-only provenance (binary address) and has no `//!`
     equivalent; it must be kept.
   - Deleting `// 3 bytes padding` between fields — this is an ABI
     fact about the struct layout and has no `//!` equivalent.
   - Folding `+0x04` field offsets into `//!<` field briefs and
     removing the trailing `// +0x04` — the offset then leaks into
     the user-facing docs site (which does not want addresses) **and**
     the standalone recon annotation is lost.
   - Replacing a multi-line `//` prose block describing a
     reconstruction observation with a `//!` brief that paraphrases
     only part of it.

### Practical layout

The two channels typically stack, with `//` reconstruction notes
nearest the declaration on the side that suits the fact:

```cpp
//! \brief Equality comparison ignoring transient fields.
//! \details Compares hold only when rate > 0.
// Reconstructed from operator!= at 0x1d5770.
// Field-wise inequality. Special: hold field only compared when rate > 0.
bool operator!=(const PlayConfig& other) const;
```

For fields, the `//!` brief goes above and the recon offset trails:

```cpp
//! \brief Sample-rate divider. JSON key `"rate"`.
int32_t rate = 0;       // +0x04
```

ABI padding is recon-only and stays inline:

```cpp
bool dummy = false;     // +0x1E
// 1 byte padding
```

---

## 15. Documentation comments inside macro bodies

Both pitfalls below were observed in practice (D5-8 SEQC_LIST work,
D5-20 BOOST_LOG_GLOBAL_LOGGER work).  Treat the rule as mandatory.

### 15.1 Inside a backslash-continued macro body — always use `/*! */`

A `//!` comment inside a backslash-continued macro body **eats every
subsequent token in the macro**.  Phase-2 line splicing concatenates
the continuation lines into one logical line *before* comment
recognition; the `//!` then extends to the end of that spliced line,
which now ends only at the macro's final non-continued newline.  The
remaining macro tokens become part of the comment and disappear from
the program.

Example of the bug:

```cpp
#define SEQC_LIST(NAME, ELEM)                              \
    class NAME : public Node {                             \
    public:                                                \
        //! \brief List of ELEM nodes.        ← BUG: eats the rest \
        std::vector<ELEM> elements_;                       \
        void accept(Visitor& v) override;                  \
    };
```

Use `/*! */` regardless of how short the brief is:

```cpp
#define SEQC_LIST(NAME, ELEM)                              \
    class NAME : public Node {                             \
    public:                                                \
        /*! \brief List of `ELEM` nodes owned by this list. */ \
        std::vector<ELEM> elements_;                       \
        void accept(Visitor& v) override;                  \
    };
```

The same applies to `///` and `/**` if those forms were ever used
(they are forbidden by §13 anyway, but the splicing hazard is the
same — single-line forms are unsafe inside macro bodies).

### 15.2 `MACRO_EXPANSION=YES` can mis-attach briefs to invocations

When `MACRO_EXPANSION=YES` is on (it is, in our Doxyfile), Doxygen
treats the macro *invocation* as the documented declaration.  A `//!`
or `/*! */` block placed elsewhere in the file may be silently
re-attached to a nearby macro invocation, especially when:

- the brief precedes a forward declaration that follows a macro
  invocation (the brief drifts upward to the invocation), or
- the brief carries `\param` / `\return` tags whose names do not
  match the macro's expanded parameter list (Doxygen still attaches
  the brief, then warns about "argument X not found").

Symptom in `doxygen-warnings.log`:

```
warning: argument 'eptr' of command @param is not found in the
argument list of zhinst::logging::BOOST_LOG_GLOBAL_LOGGER(...)
```

When a brief refuses to attach to its intended target despite being
adjacent in source, **prefer one of these workarounds**:

1. Inline the parameter / return prose into the `\details` body and
   drop the explicit `\param` / `\return` tags.  This loses the
   per-parameter table on the rendered page but eliminates the
   warning and silently-wrong attachment.
2. Move the brief immediately above the function declaration with no
   intervening blank lines or other comment blocks.
3. As a last resort, document the macro invocation itself with the
   needed brief, leaving the underlying declaration without an
   attached doc.

### 15.3 Macro-defined symbol families (`\name` groups)

Macro invocations that generate one symbol per call (e.g. the
`ZHINST_DECLARE_EXCEPTION` family, the `ZHINST_DECLARE_FACTORY`
device family, the `BOOST_LOG_GLOBAL_LOGGER` global-logger tag)
require a `//!` or `/*! */` brief on **each invocation** — the brief
inside the macro body propagates field-level documentation to the
generated members, but the class-level brief is per-invocation.

Wrap large invocation groups in `\name … \{ … \}` blocks (see §13)
so the rendered page collects them as a labelled member group rather
than a flat list.

---

## What is NOT used in source files

| Pattern | Reason removed |
|---|---|
| `// Phase NN:` / `// Step N:` (numbered workflow steps) | Workflow audit trail; no long-term technical value |
| `// GDB-verified:` / `// GDB-confirmed:` prefix | Provenance tag, not a technical fact; underlying fact retained as `// Binary:` where non-obvious |
| `CORRECTED …` notices | Correction has been applied; history is in git log and notes |
| `HISTORICAL NOTE:` blocks | Superseded content; retained in `notes/` files |
| "Hallucination" / "Removed" notices | The hallucination is gone; comment is noise |
| `// verify …` uncertainty notes | Promoted to `TODO` if still open, otherwise deleted |
| Doxygen `///` and `/** */` | Doxygen markers are `//!` and `/*! */` only — see §13 |
| Copyright / license headers | Not applicable |

---

## Notes files (`reconstructed/notes/`)

Notes files are reference documents, not source code. Comment style rules do
not apply to them. Historical content, `CORRECTED` entries, phase language, and
archaeology are preserved in notes indefinitely. The only normalization applied
to notes is `IF-NNN` tag prefixing (`NOTE:` / `TODO:`) when obviously missing.
