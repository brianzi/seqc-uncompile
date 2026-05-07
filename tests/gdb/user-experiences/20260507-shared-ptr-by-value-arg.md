# Reading shared_ptr-by-value arg via `$rsi` in a member function

**Date**: 2026-05-07
**Context**: Tracing `Prefetch::needsNewCwvf(std::shared_ptr<Node> node) const`
@ 0x1dc620 to dump the node's parent chain at function entry, for IF-158.

## What I tried first

I knew the function was a const member taking `std::shared_ptr<Node>`
by value.  Assumed System-V calling convention "this in rdi, then
shared_ptr split into rsi/rdx" and tried to dereference rsi as
`shared_ptr*`:

```python
rsi = int(gdb.parse_and_eval("$rsi"))
node = int(gdb.parse_and_eval(f"*(unsigned long*){rsi}"))
```

I expected `rsi` to be a *pointer* to the shared_ptr struct
(2 × 8 bytes: managed ptr + control block).

## What went wrong

This actually worked — but only by coincidence on this particular
build.  In libc++ (and on this binary), shared_ptr is **passed by
hidden reference** for non-trivial copy: rsi holds the address of
a caller-allocated 16-byte temporary holding `{managed_ptr,
control_block}`.  So `*(unsigned long*)rsi` correctly reads the
managed pointer (offset 0).  But the assumption "rsi/rdx hold the
two 8-byte halves directly" (per the SysV "small struct" rule) is
wrong here because the type has a non-trivial copy ctor — it's
passed indirectly.

Confusingly, `gdb.Breakpoint.stop()` also had a subtle gotcha: when
my Python class threw an exception (e.g. a typo in the
`parse_and_eval` expression), the breakpoint **still stopped the
inferior** even though `stop()` would have returned `False` on
success — because the exception path returns truthy by default.

## How I fixed it

1. For shared_ptr by value to a non-POD type, **always go through
   the indirection**: read the pointer at `*rsi` (managed ptr), then
   dereference fields off that.  The trace recipe that worked:

   ```python
   rsi = int(gdb.parse_and_eval("$rsi"))
   node = int(gdb.parse_and_eval(f"*(unsigned long*){rsi}"))
   t    = int(gdb.parse_and_eval(f"*(int*)({node}+0x44)"))
   ```

2. Wrap the whole `stop()` body in `try/except` and **always return
   `False`** in batch tracing mode, so a Python error doesn't halt the
   inferior:

   ```python
   def stop(self):
       try:
           ...
       except Exception as e:
           print(f"err: {e}")
       return False
   ```

## Take-away for next time

- shared_ptr / unique_ptr / any class-with-non-trivial-copy passed
  "by value" on x86-64 SysV is in fact passed **by hidden reference**:
  arg-register holds the address of a stack temporary containing the
  struct.  Always indirect once before treating the bytes as the
  struct contents.
- For batch GDB tracing with `gdb.Breakpoint.stop()` overrides,
  always wrap the body in `try/except` and unconditionally
  `return False` — otherwise a Python typo silently halts the trace
  and you spend minutes wondering why the script hung.
