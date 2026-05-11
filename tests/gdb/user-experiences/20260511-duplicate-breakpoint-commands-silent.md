# Duplicate breakpoints at the same address: the second `commands` block silently does not fire

**Date**: 2026-05-11
**Context**: D9.1 GDB verification of the Path B1 emission block at `0x1db1f3..0x1db55d` — wanted both a generic "HIT gate_cmp" log line and a separate breakpoint that printed the comparison values at the same address `0x1db223`.

## What I tried first

```python
# loop registered gate_cmp first
for name, off in points:
    bp = gdb.Breakpoint(f'*{base+off}')
    gdb.execute(f"""commands {bp.number}
silent
printf "\\n>>> HIT {name}\\n"
... continue
end""")

# then a SECOND breakpoint at the same address, with richer printfs
bp_gate = gdb.Breakpoint(f'*{base+0x1db223}')
gdb.execute(f"""commands {bp_gate.number}
silent
printf "\\n>>> AT gate cmp:\\n"
printf "  eax (cacheSize) = %d\\n", $eax
... continue
end""")
```

## What went wrong

On the run, only the **first** breakpoint at `0x1db223` produced output (`>>> HIT gate_cmp(...)`). The second breakpoint's `>>> AT gate cmp:` printfs never appeared. Both `Breakpoint` objects were created (different `bp.number` values), but only the first's `commands` block executed. The diagnostics I needed (the actual `eax` vs `[rcx+0xc]` values) were silently lost.

## How I fixed it

Inferred the gate's truth value indirectly from control flow: the run reached `first_prf @0x1db2b3` (which is past `jne 1db876`), so `jne` was *not* taken, so `eax == [rcx+0xc]` held. This recovered the answer (Q3 partially confirmed) without needing a second BP, but cost time chasing why the second BP was silent.

Better fix for next time: never register two `gdb.Breakpoint`s at the same address. Either (a) put all the diagnostics into a single `commands` block, or (b) parameterise a single shared printf helper that conditionally dumps extras based on the breakpoint name.

## Take-away for next time

When you want both a "trace hit" line and richer diagnostics at the same address, **merge them into one `commands` block** at one breakpoint. Setting two `gdb.Breakpoint(*ADDR)` at the same address in batch mode results in only the first commands block running; the second is a no-op without warning.
