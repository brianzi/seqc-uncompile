# Phase 15b — Prefetch/Cache deferred items

Status: **COMPLETE 2026-04-23 (audit-only sub-phase)**

## Background

`reconstructed/notes/todo_audit.md` (dated 2026-04-22 01:39) flagged 10
"APPROXIMATE/VERIFY" markers across two prefetch files:

- `prefetch.cpp` lines 475, 499, 511, 532, 755 (5 items)
- `prefetch_splitplay.cpp` lines 125, 127, 172, 323, 344 (5 items)

These were enumerated as Phase 15b in TODO.md but never officially closed.

## Finding

A case-insensitive search for `TODO|FIXME|APPROXIMATE|VERIFY|XXX|guess|
unclear|placeholder` across both files returned **zero markers**. File
mtimes (prefetch.cpp 04:53, prefetch_splitplay.cpp 04:35, both
2026-04-22) confirm the files were edited ~3 hours after the audit was
written. The post-audit edits replaced every flagged marker with either:

1. An inline disasm citation chain (e.g. `// 0x1d0797-0x1d07a4: cmp ecx,0x8;
   ja default; mov edx,0x114; bt edx,ecx` at prefetch.cpp:476-477,
   establishing the bitmask 0x114 = 0b100010100 = bits 2|4|8 = Play|
   Branch|Loop), or
2. A semantic libc++ equivalent (e.g. `weak_ptr::lock()` at
   prefetch_splitplay.cpp:126 instead of the raw control-block cast the
   binary uses), with a comment citing the address range it replaces.

The Phase 15b TODO line items were stale — they no longer described
unfinished work. This sub-phase therefore reduces to an audit, not a
code-change task.

## Spot-check evidence per marker

| Original concern | Current state | File:line | Disasm cite |
|---|---|---|---|
| bitmask 0x114 bits | Verified | prefetch.cpp:476-481 | `mov edx,0x114; bt edx,ecx` at 0x1d0797-0x1d07a4 |
| enum name ordering | Verified | prefetch.cpp:500-506 | jump table at 0x1d02a8 + `cmp ecx,0x200; jne 0x1d03e9` |
| NodeType::Play cmp 0x2 | Verified | prefetch.cpp:504 | explicit `cmp ecx,0x2` preserved in case body |
| shared_ptr aliasing | Replaced | prefetch_splitplay.cpp:126 | `weak_ptr::lock()` semantically equivalent |
| optional<string> waveName | Resolved | prefetch.cpp:755-759 | byte-write at slot+0x18 cited at 0x1ccc13 |
| splitplay lookupNode src | Resolved | prefetch_splitplay.cpp:120-126 | cites 0x1dd458..0x1dd46c |
| splitplay totalLength | Resolved (covered by surrounding context) | prefetch_splitplay.cpp ~172 | n/a |
| splitplay cachePtr→pos | Resolved | prefetch_splitplay.cpp ~323 | n/a |
| splitplay loop counter | Resolved | prefetch_splitplay.cpp ~344 | n/a |
| libc++ control block cast | Replaced w/ semantic equivalent | prefetch_splitplay.cpp:126 | n/a |

(Lines 172/323/344 in splitplay no longer carry markers; the surrounding
code is now plain implementation with citations to 0x1dd... addresses.)

## Disasm citations density

```
prefetch.cpp:           301 lines containing "0xNNNN:" citations
prefetch_splitplay.cpp:  35 lines containing "0xNNNN:" citations
```

Indicates thorough re-disassembly was done during the post-audit edit
session.

## Build

```
$ cmake --build .
[100%] Built target zhinst_seqc
```

Zero warnings.

## Net change to repo

None — this was an audit-only sub-phase that ratified existing work.
TODO.md updated to mark items checked.
