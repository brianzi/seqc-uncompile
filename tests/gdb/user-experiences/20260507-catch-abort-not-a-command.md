# `catch abort` does not exist in this gdb

**Date**: 2026-05-07
**Context**: IF-181 — needed to capture the SIGABRT backtrace from a libstdc++ debug-assertion crash inside the recon `_seqc_compiler.so`.

## What I tried first

```gdb
catch abort
run
bt 60
```

## What went wrong

```
Error in sourced command file:
Undefined catch command: "abort".  Try "help catch".
```

The `catch` command in this gdb (15.x on Arch with python3.14)
supports `catch syscall`, `catch throw`, `catch signal SIGABRT`,
etc., but **not** the bare `catch abort` form sometimes shown in
gdb tutorials.  The script aborted before getting to `run`, so
the inferior never even launched.

## How I fixed it

Replaced with a function breakpoint, which works fine because
libc's `abort` is exported and resolvable even without debug
symbols:

```gdb
break abort
run
bt 60
```

(`break __GI_abort` and `catch signal SIGABRT` both also work.)
The breakpoint reports "Function 'abort' not defined" before the
`.so` is loaded, then is auto-resolved at runtime — that warning
is harmless.

## Take-away for next time

To stop on a libstdc++ debug-assertion failure, use
`break abort` (or `catch signal SIGABRT`).  `catch abort` is not
a valid gdb command — don't copy it from old web tutorials.
