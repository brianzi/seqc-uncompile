# GDB Debugging Helper Scripts

This directory contains Python scripts and GDB command files for tracing the
original `_seqc_compiler.so` binary under GDB. These are invaluable for
understanding control flow, verifying reconstruction hypotheses, and debugging
differential test failures.

---

## Why Use GDB for Binary Analysis?

**Static disassembly is slow and error-prone.** Reading disassembly alone:
- Makes it hard to tell which branches are actually taken
- Hides register/memory values at critical points
- Requires hours of manual tracing for complex control flow

**GDB can answer in seconds what takes hours manually:**
- "Is this phase conditional on flag X?" → Set breakpoint, check
- "What is rax at this instruction?" → Print register
- "Which code path does this test take?" → Trace execution

**Default to GDB verification** whenever a control-flow assumption drives a
code change. The cost is ~30 seconds; the cost of a wrong assumption is hours.

---

## Quick Start

### 1. Write a Minimal Test Script

The original binary is a Python extension module, so we invoke it via Python:

```python
#!/usr/bin/env python3
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
wave w = zeros(64);
playWave(w);
"""

result = sc.compile_seqc(code, 'HDAWG8', {}, 0, samplerate=2.4e9)
print("OK, ELF size:", len(result[0]))
```

Save as `gdb/my_trace.py`.

### 2. Write a GDB Command File

```gdb
set pagination off
set confirm off
set auto-load safe-path /

# Wait for the .so to load
set stop-on-solib-events 1
run

# Find the base address
python
import gdb
while True:
    info = gdb.execute("info sharedlibrary _seqc_compiler", to_string=True)
    if '_seqc_compiler' in info and '0x' in info.split('_seqc_compiler')[0].split('\n')[-1]:
        for line in info.split('\n'):
            if '_seqc_compiler' in line and '0x' in line:
                base = int(line.split()[0], 16)
                break
        break
    gdb.execute("continue")

gdb.execute("set stop-on-solib-events 0")

# Set breakpoints at binary offsets
for name, off in [
    ('function_entry', 0x15e060),
    ('check_condition', 0x15e326),
]:
    bp = gdb.Breakpoint(f'*{base + off}')
    gdb.execute(f"""commands {bp.number}
silent
printf ">>> {name} at 0x{off:x}\\n"
info registers rax rbx rcx rdx rdi rsi
continue
end""")
end

continue
quit
```

Save as `gdb/my_trace.gdb`.

### 3. Run It

```bash
cd tests
gdb -batch -x gdb/my_trace.gdb --args python3 gdb/my_trace.py 2>&1 | grep ">>>"
```

**Output:**
```
>>> function_entry at 0x15e060
rax            0x7f8e3c0a2e80
>>> check_condition at 0x15e326
rax            0x1
```

---

## Key GDB Gotchas

### 1. Never Use `break FunctionName`

The .so has **no debug symbols**. `break PyInit__seqc_compiler` silently fails.

**Correct approach:**
- Use `set stop-on-solib-events 1` to catch library load
- Read the base address from `info sharedlibrary`
- Set breakpoints at `*{base + offset}`

### 2. Breakpoint Commands Must Use `continue`

For batch mode (`gdb -batch`), breakpoint commands must end with `continue`:

```gdb
python bp = gdb.Breakpoint(f"*{base + 0x15e060}")
commands
silent
printf "Hit!\n"
continue
end
```

Without `continue`, GDB stops at the first breakpoint and the script hangs.

### 3. Verify Addresses Before Trusting Breakpoints

Before trusting a breakpoint, check the first byte:

```gdb
x/1xb 0x<address>
```

Function entries typically start with `0x55` (push rbp).

### 4. libc++ String Layout (SSO)

The original binary uses libc++ (not libstdc++). `std::string` has this layout:

**Short string (≤22 bytes):**
- Byte 0, bit 0: 0 (short indicator)
- Byte 0, bits 1-7: length << 1
- Bytes 1-22: data

**Long string (>22 bytes):**
- Byte 0, bit 0: 1 (long indicator)
- Bytes 16-23: pointer to heap data
- Bytes 8-15: length

**Reading a string in GDB:**
```gdb
set $str = (char*)$rdi
set $short = *(unsigned char*)$str & 1
if $short == 0
    set $len = (*(unsigned char*)$str) >> 1
    set $data = $str + 1
else
    set $len = *(unsigned long*)($str + 8)
    set $data = *(char**)($str + 16)
end
printf "String (%d bytes): %s\n", $len, $data
```

---

## Example Scripts

### gdb_trace.py

Generic trace script for UHFLI with `playWaveIndexed`:

```python
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
wave w = placeholder(1000);
playWaveIndexed(w, 200, 128);
"""

result = sc.compile_seqc(code, 'UHFLI', {}, 0)
print("OK, ELF size:", len(result[0]))
```

### gdb_trace_hdawg.py

HDAWG-specific trace with `executeTableEntry`:

```python
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
wave w_1 = zeros(64);
wave w_2 = zeros(64);
wave w_3 = zeros(64);

assignWaveIndex(1, w_1);
assignWaveIndex(2, w_2);
assignWaveIndex(3, w_3);

while (true) {
    executeTableEntry(1);
    executeTableEntry(2);
    executeTableEntry(3);
}
"""

result = sc.compile_seqc(code, 'HDAWG8', {}, 0, samplerate=2.4e9)
print("OK, ELF size:", len(result[0]))
```

### gdb_trace_multi.py

Multi-waveform test:

```python
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

elf, extra = sc.compile_seqc(
    'wave w1 = zeros(64);\nwave w2 = ones(64);\nplayWave(w1);\nplayWave(w2);',
    'HDAWG8', '', 0, samplerate=2.4e9)
print(f"ELF size: {len(elf)}")
print(f"Messages: {extra.get('messages', '')}")
```

### gdb_playzero.py

Trace `playZero` with a ternary (Var) argument:

```python
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

source = '''var c = getUserReg(0);
playZero((c > 0) ? 32 : 64);
'''
result = sc.compile_seqc(source, 'HDAWG8', {}, 0, samplerate=2.4e9)
print("OK:", len(result) if result else "None")
```

---

## Common Debugging Workflows

### Workflow 1: "Is This Phase Conditional?"

**Question:** Is `Prefetch::optimizeSync()` conditional on `multiValue`?

**GDB approach:**

1. Write a test that should trigger the phase
2. Set breakpoints at entry and return
3. Run, check if breakpoints hit
4. Compare with a test that shouldn't trigger it

**Example:**

```gdb
# Break at Prefetch::optimizeSync entry (0x1ce110)
python bp = gdb.Breakpoint(f"*{base + 0x1ce110}")
commands
silent
printf ">>> optimizeSync() called\n"
continue
end
```

Run with different test cases. If it's called for both single and multi
waveforms, it's unconditional.

### Workflow 2: "What Is This Register Value?"

**Question:** At this `cmp rax, rdx`, what is rax?

**GDB approach:**

```gdb
python bp = gdb.Breakpoint(f"*{base + 0x15e326}")
commands
silent
printf ">>> At cmp: rax=%lx rdx=%lx\n", $rax, $rdx
continue
end
```

### Workflow 3: "Which Branch Is Taken?"

**Question:** After this conditional jump, which path is taken?

**GDB approach:**

```gdb
# Break at both targets
python gdb.Breakpoint(f"*{base + 0x15e330}")  # true branch
python gdb.Breakpoint(f"*{base + 0x15e400}")  # false branch

commands
silent
printf ">>> True branch taken\n"
continue
end

commands
silent
printf ">>> False branch taken\n"
continue
end
```

### Workflow 4: "What Does This Memory Point To?"

**Question:** rdi points to a string — what is it?

**GDB approach:**

```gdb
python bp = gdb.Breakpoint(f"*{base + 0x15e060}")
commands
silent
printf ">>> String at rdi: %s\n", (char*)($rdi + 1)
# Assumes short string, adjust for long strings
continue
end
```

---

## Lessons Learned

### Lesson 1: Phase 5 Was Unconditional

**Hypothesis:** `mergeWaveforms` phase 5 is conditional on `multiValue`.

**Static analysis:** Looked conditional from disassembly.

**GDB trace:** Proved it's unconditional — saved hours of wrong-path debugging.

### Lesson 2: Cache Logic Was Inverted

**Hypothesis:** `getOrCreateWaveform` cache hit means "use cached value".

**GDB trace:** Actually, cache hit means "call factory again" (inverted logic).

**Result:** Fixed in minutes instead of hours of confusion.

### Lesson 3: Default to Verification

**Principle:** Whenever a control-flow assumption drives a code change, verify
it with GDB first.

**Cost:** ~30 seconds to set up a trace.

**Benefit:** Avoids hours spent implementing the wrong behavior.

---

## Advanced Techniques

### Conditional Breakpoints

```gdb
python
bp = gdb.Breakpoint(f"*{base + 0x15e060}")
bp.condition = "$rax == 0x42"
end
```

Only stops when rax = 0x42.

### Watchpoints

Break when a memory location changes:

```gdb
watch *(int*)0x7ffff7abc000
```

### Python-Powered Breakpoints

Full Python control over breakpoint behavior:

```python
class MyBreakpoint(gdb.Breakpoint):
    def stop(self):
        rax = gdb.parse_and_eval("$rax")
        if rax > 0:
            print(f"rax is positive: {rax}")
            return False  # Don't stop
        return True  # Stop here

MyBreakpoint(f"*{base + 0x15e060}")
```

### Tracing Function Calls

Use `catch syscall` or `rbreak` to trace all calls matching a pattern (requires
debug symbols, not applicable here).

For binary without symbols, use offset-based breakpoints for all suspected
call sites.

---

## Troubleshooting

### "No symbol table is loaded"

Expected — the .so has no debug symbols. Use offsets from disassembly.

### "Cannot access memory at address 0x..."

The address is wrong or the memory isn't mapped yet. Verify:
1. Base address is correct (`info sharedlibrary`)
2. Breakpoint offset is correct (check disassembly)
3. Code has executed past initialization

### Breakpoint Never Hits

Check:
1. Test input actually triggers the code path
2. Offset is correct (verify with `objdump -d`)
3. Base address is correct
4. GDB command file has `continue` in breakpoint commands

### String Looks Garbled

You're reading it with the wrong string layout assumption (short vs long).
Check byte 0, bit 0 to determine layout.

---

## Tips

1. **Start simple:** Set one breakpoint, verify it hits, then expand.
2. **Use `silent`:** Keeps output clean in batch mode.
3. **Filter output:** Pipe to `grep ">>>"` to see only your trace points.
4. **Save command files:** Reusable for similar debugging sessions.
5. **Combine with static analysis:** Use GDB to verify hypotheses from
   disassembly, not to explore blindly.

---

## Further Reading

- [GDB Manual: Python API](https://sourceware.org/gdb/onlinedocs/gdb/Python-API.html)
- [GDB Cheat Sheet](https://darkdust.net/files/GDB%20Cheat%20Sheet.pdf)
- See `AGENTS.md` in repo root for detailed GDB tracing recipes
