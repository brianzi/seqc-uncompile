# GDB user-experience reports

When you (an agent) use GDB to investigate a recon bug and **anything
about the GDB setup itself trips you up**, log it here.  The goal is
to grow the GDB reference material in `../README.md` and `AGENTS.md`
to anticipate these hiccups for future agents.

## When to log

- A breakpoint did not fire when you expected it to.
- A breakpoint command syntax error (missing `continue`, wrong scope,
  etc.) caused the script to hang or stop early.
- An address calculation was wrong (forgot to add base, used the
  wrong base, miscounted offset).
- A register / memory dereference returned junk (wrong type, wrong
  ABI, wrong string layout).
- The Python-extension launch dance (`stop-on-solib-events`,
  base-address discovery) misbehaved.
- A subtle GDB / libstdc++ / libc++ / Python interaction bit you.
- You found a more efficient pattern than what `../README.md`
  suggests.

If your GDB session worked first try with no surprises, **don't
log anything** — these reports exist to surface friction, not to
accumulate trivia.

## How to log

Write a single Markdown file named `YYYYMMDD-short-topic.md` here.
No frontmatter required.  Suggested skeleton:

```markdown
# <short title — one line>

**Date**: YYYY-MM-DD
**Context**: <which IF / test you were debugging, in one sentence>

## What I tried first

<the wrong / wasteful approach, with the actual command(s) you ran>

## What went wrong

<the symptom: hang, wrong output, error message, silent failure>

## How I fixed it

<the corrected approach, with the actual command(s)>

## Take-away for next time

<one or two sentences distilling the lesson>
```

Keep it short — 30-80 lines is plenty.  The point is the lesson,
not a transcript.

## Periodic consolidation

When this directory grows past ~10 reports, the next agent doing a
sub-phase wrap-up should fold the recurring lessons into
`../README.md` and `AGENTS.md` (under "GDB tracing for binary
analysis"), and archive the consumed reports under
`./archive/`.
