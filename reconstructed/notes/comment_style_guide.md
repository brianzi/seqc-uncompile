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

## What is NOT used in source files

| Pattern | Reason removed |
|---|---|
| `// Phase NN:` / `// Step N:` (numbered workflow steps) | Workflow audit trail; no long-term technical value |
| `// GDB-verified:` / `// GDB-confirmed:` prefix | Provenance tag, not a technical fact; underlying fact retained as `// Binary:` where non-obvious |
| `CORRECTED …` notices | Correction has been applied; history is in git log and notes |
| `HISTORICAL NOTE:` blocks | Superseded content; retained in `notes/` files |
| "Hallucination" / "Removed" notices | The hallucination is gone; comment is noise |
| `// verify …` uncertainty notes | Promoted to `TODO` if still open, otherwise deleted |
| Doxygen (`///`, `/** */`) | Not used; no API documentation target |
| Copyright / license headers | Not applicable |

---

## Notes files (`reconstructed/notes/`)

Notes files are reference documents, not source code. Comment style rules do
not apply to them. Historical content, `CORRECTED` entries, phase language, and
archaeology are preserved in notes indefinitely. The only normalization applied
to notes is `IF-NNN` tag prefixing (`NOTE:` / `TODO:`) when obviously missing.
