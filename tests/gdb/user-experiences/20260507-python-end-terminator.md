# `continue` inside `python ... end` block clobbers GDB's `continue` command

**Date**: 2026-05-07
**Context**: Tracing `Prefetch::needsNewCwvf` parent-chain on the original
binary for IF-158.

## What I tried first

A GDB command file with a `python` block that defines a
`gdb.Breakpoint` subclass, followed by GDB's `continue` to actually
run the inferior:

```gdb
python
import gdb
class TraceNNC(gdb.Breakpoint):
    def stop(self):
        # ... print stuff ...
        return False  # don't actually stop

addr = base + 0x1dc620
TraceNNC(addr)

continue
quit
```

(no `end` between the Python body and the `continue` line).

## What went wrong

GDB tried to source `continue` and `quit` as **Python statements**
because the `python` block was never terminated.  `continue` outside
a Python loop is a syntax error:

```
Python Exception <class 'SyntaxError'>: 'continue' not properly in loop
/tmp/gdb_if158_v3.txt:59: Error in sourced command file:
Error occurred in Python: 'continue' not properly in loop (<string>, line 49)
```

The whole script aborted before any breakpoint was set.

## How I fixed it

Add the `end` terminator on its own line **between** the Python body
and the GDB-shell `continue`:

```gdb
python
...
TraceNNC(addr)
end

continue
quit
```

## Take-away for next time

Every `python` block in a GDB command file needs an explicit `end`
line.  A blank line is **not** a terminator.  If your Python block
ends with arbitrary statements (not a function/class definition that
visually "closes"), the missing `end` is easy to overlook — and the
error message points at the *Python* line number, not the offending
GDB line, so the diagnosis is non-obvious.
