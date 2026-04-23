# libc++ vs libstdc++ ABI patterns

This binary (`_seqc_compiler.so`) is compiled against **libc++** (the
LLVM/Clang C++ standard library, `std::__1::*` inline namespace),
while our reconstructed source is built against the host's **libstdc++**
(`std::__cxx11::*` inline namespace).

Several standard-library types differ in size and internal layout
between the two implementations. This document catalogs the
differences relevant to layout reconstruction work.

## Type sizes (x86_64 SysV)

| Type | libc++ (`std::__1::`) | libstdc++ (`std::__cxx11::`) | Notes |
|---|---|---|---|
| `basic_string<char>` | **24** bytes (3 × 8) | **32** bytes (4 × 8) | libc++ uses a tagged union with a 1-byte SSO discriminator; libstdc++ uses local-buffer + size + capacity + ptr |
| `vector<T>` | 24 bytes (3 ptrs) | 24 bytes | Compatible |
| `shared_ptr<T>` | 16 bytes (raw + ctrl) | 16 bytes | Compatible |
| `weak_ptr<T>` | 16 bytes (raw + ctrl) | 16 bytes | Compatible |
| `__tree<...>` head (used by `std::map`/`std::set`) | **0x30 (48)** bytes | **0x30 (48)** bytes (since GCC's RB tree also has 6 words) | Sizes match but internal node layout differs |
| `std::map<K,V>` | 0x30 bytes (== `__tree`) | 0x30 bytes | Compatible at top level |
| `function<R(Args...)>` | 0x30 bytes | 0x20 bytes | libc++ has a larger SBO buffer |
| `basic_ifstream<char>` | 0x1c8 bytes | varies | embedded `basic_filebuf` |
| `bad_exception` | 0x10 bytes (vptr + ?) | 0x10 bytes | Compatible |

The string and `function` differences are the big ones.

## libc++ basic_string layout (24 bytes)

libc++ uses a **tagged short-string-optimization** layout. The first
byte's low bit distinguishes long (heap) from short (inline) form:

```
short form (≤22 chars):
  +0x00  uint8_t  (size << 1)        // bit 0 = 0
  +0x01  char[22] inline data
  +0x17  char     '\0' terminator (often)

long form:
  +0x00  uint64_t (capacity << 1) | 1   // bit 0 = 1
  +0x08  uint64_t size
  +0x10  char*    data
```

This is why disassembly of std::string consumers consistently shows:

```asm
test BYTE PTR [rdi], 0x1     ; check long bit
je   short_form
mov  rsi, [rdi]              ; capacity
mov  rdi, [rdi+0x10]         ; data ptr
and  rsi, 0xfffffffffffffffe ; clear bit 0 → real capacity
call operator delete
```

## Implications for reconstruction

### 1. We cannot host-`static_assert` struct sizes

For any struct that contains a `std::string`, `std::map`, or
`std::function`, the host-compiled size will differ from the binary.
We must:
- document the size in a comment (verified from disasm)
- comment out or omit `static_assert(sizeof(X) == N)` lines, or wrap
  them under `#ifdef _LIBCPP_VERSION`

Examples in our headers where this matters:
- `cached_parser.hpp` (size 0x60: contains 1 map + 2 strings)
- `awg_assembler_impl.hpp` (size 0x170: contains many strings)
- `error_messages.hpp` ZIAWGCompilerException (size 0x60: inherits a
  string-bearing zhinst::Exception)
- `value.hpp` Value (size 0x18: contains a std::string in storage_)

### 2. Placement-new of string into too-small storage

`value.cpp:182` triggers a host-compiler warning:

```
warning: placement new constructing an object of type 'std::string'
         and size '32' in a region of type 'Value::Storage' and size '24'
```

This is a **host/binary ABI mismatch artifact, not a real bug**: in the
binary, `Value::Storage` is the libc++ string's 24 bytes, which is
exactly correct. On the host, libstdc++'s string is 32 bytes and
overflows. The warning is benign but noisy; we keep it as the only
warning to make any new errors easy to spot.

### 3. Field accessors crossing the libc++/libstdc++ boundary

For methods on libc++ strings that we call from reconstructed source
(`c_str()`, `length()`, etc.) the source-level API is identical, so we
can use them freely. Only the **layout** differs, not the interface.

### 4. Embedded std::map size

`CachedParser::index_` is a `std::map<vector<uint>, CacheEntry>` and
occupies exactly 0x30 bytes inside CachedParser (offset +0x00..+0x30).
Both libc++ and libstdc++ implement std::map as a red-black tree with
an "end node" head plus a size_t — both happen to be 0x30 bytes total
on x86_64. So this layout is preserved across libraries; what differs
is allocator behavior and node layout (irrelevant for top-level size).

## shared_ptr / weak_ptr

Both implementations use:

```
+0x00  T*               raw pointer (or aliased pointer)
+0x08  __shared_weak_count*  control block pointer (or nullptr)
```

The control block layout differs:

**libc++ (`std::__1::__shared_weak_count`):**
```
+0x00  vptr
+0x08  long  __shared_owners_   (strong count - 1)
+0x10  long  __shared_weak_owners_ (weak count + #strong refs - 1)
```

The "minus 1" convention means a fresh `make_shared<T>` writes:
- `__shared_owners_ = 0` (one strong owner)
- `__shared_weak_owners_ = 0` (zero weak + one strong = 0 in this rep)

Decrement uses lock-free fetch_sub; when a counter goes negative,
the relevant phase fires. Disasm pattern:

```asm
lock dec QWORD PTR [rcx+0x8]   ; strong release
js   __on_zero_shared          ; if went negative → destroy object
```

This is why the Node weak_ptr decomposition (Node +0x18/+0x20) shows
two separate qword fields instead of an opaque 16-byte blob.

**libstdc++ (`__shared_count`):**
```
+0x00  vptr
+0x08  _Atomic_word   _M_use_count
+0x0C  _Atomic_word   _M_weak_count
```

Different size and layout — but again, only the control block, not the
shared_ptr/weak_ptr "handle" itself.

### `__on_zero_shared_weak` deallocation size pattern

When the weak count hits zero, the destructor calls
`operator delete(this, N)` where N is the **size of the controlled
block** (the emplace buffer). This gives a reliable way to determine
struct size from the binary even when a class has no other allocation
sites:

```asm
mov  esi, 0xc0          ; sizeof(__shared_ptr_emplace<X>) = 0xC0
                        ; → sizeof(X) ≤ 0xC0 - 0x18 (ctrl block) = 0xA8
call operator delete
```

This pattern was used to confirm `AsmExpression` size (0xA8) at
0x2877db, and many others.

## When to trust which size

| Situation | Trust source |
|---|---|
| Class with no string/map/function/ifstream members | Match across libs — host `sizeof` is fine |
| Class with std::string members | Disassembly only |
| Class with std::map/std::set | Disassembly (top-level size matches but node layout doesn't) |
| Class with std::function | Disassembly |
| Class derived from std::exception | Disassembly (multiple inheritance with boost::exception varies) |

## Refactoring pattern: raw+ctrl pair → weak_ptr (Phase 10.8a)

Earlier reconstruction passes sometimes modeled an embedded weak_ptr
field as two separate raw pointers (e.g. `Node* foo_ptr` + `void*
foo_ctrl`) to reflect the binary layout literally, then used
`shared_ptr<Node>(raw, [](Node*){})` workarounds at use sites to lock
the (alleged) reference. This is incorrect:

- It loses the lifetime semantics (the no-op deleter doesn't actually
  participate in the weak/strong refcount dance).
- It breaks if the referent is deleted between the read and the use.
- It triggers a cascade of awkward call sites.

The correct pattern: declare the field as `std::weak_ptr<Node> foo`
(16 bytes, exactly matching `[Node*; __shared_weak_count*]` layout),
and at use sites write `if (auto sp = node->foo.lock()) { ... }`.
This was applied to `Node::loadRef` in Phase 10.8a, fixing
~12 call sites across 5 files (prefetch.cpp, prefetch_print.cpp,
prefetch_splitplay.cpp, prefetch_placesingle.cpp, node.cpp).

Watch for this pattern in remaining files: any `_ptr`/`_ctrl` field
pair with 16-byte total footprint inside a class with weak/shared
semantics is almost certainly a `weak_ptr<T>`.

## See also

- `notes/struct_layouts.md` — verified offset tables for major structs
- `notes/node_tree_structure.md` — example of weak_ptr decomposition
- `notes/unknowns.md` — open layout questions
