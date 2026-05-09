# Reconstructed SeqC Compiler — Documentation System

This directory holds the Doxygen + Doxygen Awesome CSS configuration that
renders the reconstructed compiler's headers and sources into a browsable
HTML site.  This README is the authoritative how-to for the doc system;
`AGENTS.md` carries a short pointer to it.

## Layout

| Path | Purpose |
|------|---------|
| `Doxyfile.in` | CMake-templated Doxygen config (uses `@VAR@` placeholders). |
| `architecture.md` | Mainpage stub.  Phase D1 will flesh this out. |
| `theme/doxygen-awesome.css` | Vendored Doxygen Awesome v2.3.4 stylesheet. |
| `theme/custom.css` | Local overrides (dark-mode tweaks, etc.). |
| `coverage.sh` | One-liner reporter for documented-symbol percentage and `\unclear` / `\verifyme` / `\binarynote` backlog. |
| `README.md` | This file. |

The build output lives at `reconstructed/build/docs/`:

- `html/` — rendered site (open `index.html`)
- `xml/` — XML metadata (used by `coverage.sh`; future Sphinx/Breathe input)
- `doxygen-warnings.log` — coverage tracker (every undocumented symbol
  appears once)

The build directory is in `.gitignore`; only `docs/` sources are
versioned.

## Building the site

From `reconstructed/build/`:

```bash
cmake --build . --target docs
```

The `docs` cmake target is registered automatically when Doxygen is
installed.  If Doxygen is missing, cmake prints a status line and the
target is omitted (the rest of the build is unaffected).

To open the rendered site:

```bash
xdg-open reconstructed/build/docs/html/index.html
```

## Coverage tracking

Run from the repo root after a `make docs`:

```bash
reconstructed/docs/coverage.sh
```

Output looks like:

```
Documentation coverage:
  Symbols documented   : 4/2712 (0.1%)
  Undocumented warnings: 2708 total
    members            : 2103
    compounds          : 412
    parameters         : 193
Open backlog tags:
  \unclear    : 0
  \verifyme   : 0
  \binarynote : 0
```

The "documented" count comes from XML `<memberdef>` entries with a
non-empty `<briefdescription>`.  `xmlstarlet` is preferred; the script
falls back to a Python regex parser if it is missing.

`WARN_IF_UNDOCUMENTED` is currently `NO` (Phase D0/D1) so the warning
log only flags doc-syntax errors and parameter mismatches.  It will be
flipped to `YES` in Phase D2 once class-level briefs land, at which
point the log itself becomes the coverage tracker.

## Comment markers — what to write

The full rules live in
`reconstructed/notes/comment_style_guide.md` §13.  Short version:

| Marker | Use |
|--------|-----|
| `//!` | Single-line documentation comment.  Becomes `\brief` thanks to `JAVADOC_AUTOBRIEF`. |
| `/*! ... */` | Multi-line documentation comment. |
| `//` | Reconstruction notes (binary offsets, ABI quirks, design rationale).  **Not** picked up by Doxygen. |

Doxygen `///`, `/** */`, and `//<` are forbidden — see the comment
style guide §"What is NOT used".  This keeps the line between
"reverse-engineering note" and "user-facing documentation" sharp.

### Example

```cpp
//! Build the per-CL prefetch plan for a single command list.
//!
//! Walks the CL's instruction stream and assigns each waveform to a
//! prefetch slot.  See `notes/prefetch.md` for the slot-allocation
//! algorithm.
//!
//! \param cl  the command list to plan for; must already be lowered.
//! \return    a plan whose `slots_` field is sorted by issue cycle.
//! \binarynote The original binary uses a stable sort here; we use
//!             `std::stable_sort` to match.
PrefetchPlan PrefetchPlanner::plan(CommandList const& cl);
```

## Custom aliases — accuracy discipline

Three project-specific tags carry the reconstruction discipline.  Each
generates a dedicated cross-reference page so the entire backlog is
discoverable from the rendered site.

| Tag | Page | Use when... |
|-----|------|-------------|
| `\unclear` | `unclear_list.html` | Documentation gap.  We don't yet understand what the symbol does or why it exists. |
| `\verifyme` | `verifyme_list.html` | We have a hypothesis but it has **not** been GDB- or test-verified.  A stronger statement than `\unclear`: it commits to a guess that needs checking. |
| `\binarynote` | `binarynote_list.html` | A verified fact about the original binary that diverges from idiomatic C++ (e.g. unusual sort order, ABI quirk, deliberately-preserved bug). |

Two prose helpers:

| Tag | Renders as |
|-----|-----------|
| `\reconstructed` | `Reconstructed from _seqc_compiler.so` |
| `\reconstructed{0xADDR}` | `Reconstructed from binary @0xADDR` |
| `\seenotes{file.md}` | `See notes` link to `reconstructed/notes/file.md` |
| `\verified{<text>}` | `Verified` paragraph with the supplied text |

### When to use which tag

- "I don't know what this does" → `\unclear`
- "I think it does X, but I haven't proven it" → `\verifyme`, with the
  hypothesis stated explicitly
- "It deliberately does X, which looks wrong but matches the binary" →
  `\binarynote`, citing the binary address or notes file

Never invent details to fill a doc comment.  Use the tags.

## Theme

Doxygen Awesome CSS v2.3.4 is vendored at
`theme/doxygen-awesome.css`.  Local overrides (dark-mode contrast fixes,
accent tweaks) live in `theme/custom.css` and are loaded after the main
theme via `HTML_EXTRA_STYLESHEET`.

Update procedure for the vendored CSS:

```bash
curl -L -o reconstructed/docs/theme/doxygen-awesome.css \
  https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.3.4/doxygen-awesome.css
```

Bump the version pin in this README and in the file's leading comment
when doing so.

`HTML_COLORSTYLE` is `AUTO_LIGHT` — the site follows the OS's preferred
colour scheme.  Dark-mode-specific overrides go in `theme/custom.css`
under a `html.dark-mode { ... }` selector.

## Phase plan

`TODO.md` carries the live plan.  Brief recap:

- **D0** (done) — framework: Doxyfile, theme, mainpage stub, cmake
  target, coverage script, AGENTS.md/notes hooks.
- **D1** — flesh out `architecture.md` with the top-level component
  map and entry points.
- **D2** — class-level `\brief` lines for every public type; flip
  `WARN_IF_UNDOCUMENTED = YES`.
- **D3** — public-method documentation pass (front-end / lowering /
  codegen / runtime / waveform).
- **D4** — internal documentation for non-trivial private methods and
  free functions.
- **D5** — close the `\unclear` / `\verifyme` backlog.
- **D6** — optional Sphinx/Breathe integration if a richer site is
  needed.

## Anti-patterns

- **Do not document a symbol you don't understand.**  Write a `\unclear`
  stub instead.  An incorrect doc comment is worse than no doc comment
  because it gets indexed and trusted.
- **Do not commit a doc comment that is just a paraphrase of the
  identifier name.**  `//! Get the name.` for `getName()` adds noise
  without information; either say something substantive or omit.
- **Do not migrate `//` reconstruction notes to `//!` wholesale.**
  Reconstruction notes describe how we figured things out from the
  binary; doc comments describe how callers should use the API.
  Mixing the two pollutes both.
- **Do not edit the vendored `theme/doxygen-awesome.css`.**  Put
  overrides in `theme/custom.css` so the vendor file stays a clean
  pin.
