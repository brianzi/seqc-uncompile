# Phase E — Diff-test harness for binding-unreachable reconstructions

This file is the working scratchpad for Phase E.  It is updated as
each sub-task progresses.  Folded into a permanent topic file
(probably alongside `diagnostics_text.md`) at Phase E wrap-up.

## E1 — Symbol inventory

### In-scope symbols

The harness exercises functions that are reachable from the
reconstructed shared object but **not** reachable through the
`_seqc_compiler.compile_seqc(...)` Python entry point, and therefore
not validated by the existing `tests/diff_test_fast.py` suite.

#### D16 cluster — `core/diagnostics_text.{hpp,cpp}` — 20 functions

All 20 carry `\verifyme`; two carry `\binarynote` for preserved
binary quirks (xmlEscapeUtf8Critical, sanitizeInvalidFilename).
Binary addresses are inside `_seqc_compiler.so` `.text`
(VMA == file offset; `.text` base = `0x0cd000`).

| # | symbol                    | offset      | mangled (libc++ / `__1`)                                                              | signature (post-mangling-resolution) |
|---|---------------------------|-------------|---------------------------------------------------------------------------------------|--------------------------------------|
| 1 | `xmlUnescape`             | `0x2fadd0`  | `_ZN6zhinst11xmlUnescapeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`             | `void(string&)` in-place             |
| 2 | `xmlUnescapeCopy`         | `0x2fcba0`  | `_ZN6zhinst15xmlUnescapeCopyENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`          | `string(string)` by-value            |
| 3 | `entityNumberToTxt`       | `0x2f4e90`  | `_ZN6zhinst17entityNumberToTxtERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`      | `string(const string&)`              |
| 4 | `entityNameToNumber`      | `0x2f4290`  | `_ZN6zhinst18entityNameToNumberERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`     | `string(const string&)`              |
| 5 | `linkToQuery`             | `0x2f2f20`  | `_ZN6zhinst11linkToQueryERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`            | `string(const string&)`              |
| 6 | `queryToLink`             | `0x2f4030`  | `_ZN6zhinst11queryToLinkERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`            | `string(const string&)`              |
| 7 | `escapeStringForCsharp`   | `0x2f9df0`  | `_ZN6zhinst21escapeStringForCsharpENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`    | `string(string)` by-value            |
| 8 | `escapeStringForJson`     | `0x2f89b0`  | `_ZN6zhinst19escapeStringForJsonERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`     | `void(string&)` in-place             |
| 9 | `escapeStringForPython`   | `0x2f9780`  | `_ZN6zhinst21escapeStringForPythonENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`    | `string(string)` by-value            |
| 10| `sanitizeFilename`        | `0x2fcbe0`  | `_ZN6zhinst16sanitizeFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`        | `void(string&)` in-place             |
| 11| `sanitizeInvalidFilename` | `0x2fd3f0`  | `_ZN6zhinst23sanitizeInvalidFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE` | `void(string&)` in-place             |
| 12| `replaceUnit`             | `0x2f7ae0`  | `_ZN6zhinst11replaceUnitERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_`      | `string(const string&, const string&, const string&)` |
| 13| `browseTo`                | `0x2eb950`  | `_ZN6zhinst8browseToENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`                  | `void(string)` by-value — **side-effect only (xdg-open)**, see notes |
| 14| `truncateUtf8Safe`        | `0x2fca40`  | `_ZN6zhinst16truncateUtf8SafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm`       | `void(string&, size_t)` in-place     |
| 15| `truncateXmlSafe`         | `0x2fc690`  | `_ZN6zhinst15truncateXmlSafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm`        | `void(string&, size_t)` in-place     |
| 16| `xmlEscapeUtf8Critical`   | `0x2faaa0`  | `_ZN6zhinst21xmlEscapeUtf8CriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`   | `void(string&)` in-place             |
| 17| `xmlEscapeCritical`       | `0x2fa7e0`  | `_ZN6zhinst17xmlEscapeCriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`       | `void(string&)` in-place             |
| 18| `generateSfc`             | `0x2d10b0`  | `_ZN6zhinst11generateSfcERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_`         | `FeaturesCode(const string&, const string&)` — POD return |
| 19| `toCheckedString`         | `0x2f2700`  | `_ZN6zhinst15toCheckedStringEPKc`                                                                       | `string(const char*)`                |
| 20| `quote`                   | `0x2fa6a0`  | `_ZN6zhinst5quoteERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`                    | `void(string&)` in-place — body in `platform.cpp:193` |

All mangled names above are **verified** by `nm -D --defined-only
libdiagtxt_libcxx.so | c++filt` after the libc++ test build was
successfully produced (Phase E2 step 1).

`generateSfc` returns `sfc::FeaturesCode` which is a trivially
copyable 8-byte POD (`uint64_t value;`) — no extra ABI work needed
beyond the regular function return convention (RAX).

`browseTo` has no return value and its only observable behaviour is
spawning `xdg-open`.  Diff-testing it usefully requires intercepting
the `fork`/`execvp` rather than comparing return values; treat as
**out of scope for E2's first pass**, may revisit in E3.

#### Non-D16 leftovers from D11..D19

Audit of 30 `\verifyme`/`\unverifiable` markers across recon (7
non-D16):

| location                                         | nature                              | E2 in scope? |
|--------------------------------------------------|--------------------------------------|--------------|
| `runtime/resources.hpp:1118`                     | declaration, no body                 | no — nothing to call |
| `codegen/prefetch.hpp:1138` + `prefetch_placesingle.cpp:1109` | `Table` arm of one branch | deferred — needs full prefetch input fixture, complex |
| `io/elf_reader.hpp:228`                          | field write-site that no path takes  | no — not callable |
| `infra/logging.hpp:69`                           | enum integer mapping                 | no — not callable |
| `asm/asm_expression.hpp:243` + `.cpp:205`        | `AsmExpression::toString()` recursion | candidate, deferred — needs ctor of AsmExpression tree |

E2's first pass therefore covers **only the D16 cluster** (19
callable, `browseTo` excluded).  The deferred candidates can be
added incrementally if the harness shape allows it.

### Inputs corpora — design

Per-symbol input lists live alongside the harness sources (likely
`tests/diff_unreachable/inputs/<symbol>.json` or similar).  Common
classes of input to drive each symbol:

- ASCII-only edge cases: empty, single byte, ascii-printable, all
  control characters.
- Whitespace edge cases: trailing nul, leading/trailing whitespace.
- UTF-8: 1/2/3/4-byte codepoints, BOM, malformed sequences,
  overlong encodings, lone surrogates as 3-byte UTF-8.
- High-byte (0x80..0xFF) sequences — drives sign-extension paths
  in `xmlEscapeUtf8Critical` and friends.
- Long inputs (>22 bytes) to exercise libc++ short→long string
  promotion on output.
- Symbol-specific inputs:
  - `xmlUnescape` / `xmlUnescapeCopy`: `&amp;`, `&lt;`,
    `&#xff;` (hex), `&#255;` (decimal), `&unknownentity;`,
    **`&amp;amp;`** (target of IF-262 secondary-pass divergence),
    nested escapes, malformed `&...` runs.
  - `entityNumberToTxt`: `&#0;` to `&#1114111;` boundaries,
    decimal vs hex (`&#x...;`), out-of-range, negative.
  - `entityNameToNumber`: every name from the recon `kTable`,
    plus unknown names.
  - `escapeStringFor{Json,Python,Csharp}`: special chars
    `\`, `"`, `\n`, `\r`, `\t`, `\b`, `\f`, `\u0000`,
    embedded nuls, codepoints requiring `\uXXXX` escape.
  - `sanitizeFilename` / `sanitizeInvalidFilename`: Windows
    reserved devices (CON, PRN, AUX, NUL, COM1..COM9,
    LPT1..LPT9), illegal characters (`<>:"/\|?*`), trailing
    dots/spaces, very long names (>255).
  - `truncate{Utf8,Xml}Safe`: cuts that land mid-codepoint,
    cuts inside `&amp;`, exact-fit and over/under by 1 byte.
  - `xmlEscapeCritical`: idempotency on already-escaped
    `&amp;`, `&lt;`, `&gt;`, `&#NNN;`.
  - `xmlEscapeUtf8Critical`: high-byte signed-extension
    behaviour (the `\binarynote` quirk).
  - `linkToQuery` / `queryToLink`: percent-encoding round-trip
    expectations, reserved characters, query separators.
  - `replaceUnit`: occurrences inside vs outside `\Q...\E`
    regex literals.
  - `generateSfc`: `MF-DEV4123` family with assorted option
    strings; non-MF families (should throw — error-comparison
    needs separate path).
  - `quote`: empty, ASCII, embedded quotes.
  - `toCheckedString`: nullptr, empty C string, normal C string.

### Out of scope for E2 first pass

- `browseTo` — spawns external process; requires fork interception.
- `generateSfc` throwing path — requires exception-vs-exception
  comparison; tackle in a follow-up if the happy path lands cleanly.
- The deferred non-D16 candidates above.

## E2 — Harness implementation (in progress)

### E2a status (done 2026-05-12)

8 sret return-by-value symbols added: `toCheckedString`,
`entityNumberToTxt`, `entityNameToNumber`, `linkToQuery`,
`queryToLink`, `xmlUnescapeCopy`, `escapeStringForCsharp`,
`escapeStringForPython`.  All 640 cases (17 symbols total) pass on
first attempt — no recon divergences surfaced by E2a.

ABI shapes implemented:

| kind         | C++ signature                | ctypes lowering                                 |
|--------------|------------------------------|-------------------------------------------------|
| `sret_cstr`  | `string f(const char*)`      | `void(slot, c_char_p)` — None ⇒ NULL            |
| `sret_cref`  | `string f(const string&)`    | `void(slot, string*)`                           |
| `sret_byval` | `string f(string)`           | `void(slot, string*)` — callee owns destruction |

`slot` is a 24 B raw allocation produced by
`diff_unreachable_string_alloc_uninit()`; the callee
placement-constructs a `std::string` into it.  After reading the
result via `string_data` / `string_size`, the harness calls
`destroy_in_place` (runs `~basic_string`) then `free_uninit`
(reclaims the raw 24 B).

For `sret_byval` the argument is also a slot, populated by
`diff_unreachable_string_place_construct(slot, data, n)`.  The
callee runs the argument's destructor; the harness only frees the
raw 24 B afterwards.

Deferred for a possible E2c:
- `replaceUnit(const string&, const string&, const string&)` —
  3-arg `sret_cref`.  Trivial extension; not run yet because the
  D16 first-pass goals are met.
- `generateSfc(const string&, const string&) -> FeaturesCode` —
  POD-returning, plus a throwing path on non-MF families.
  Different return convention (RAX, not sret); separate machinery.

### Feasibility milestone (verified)

A minimal libc++ build of the in-scope D16 surface compiles and
links cleanly:

```
mkdir -p reconstructed/build-libcxx-test
cd reconstructed/build-libcxx-test
BOOST_DIR=/home/brian/.conan2/p/b/boosta*/p
CXX="/usr/lib/llvm20/bin/clang++ -stdlib=libc++ -fPIC -O2 -std=c++20 \
     -I ../include -I $BOOST_DIR/include"
for src in core/diagnostics_text core/platform core/exception \
           device/device_type device/mf_sfc; do
  $CXX -c ../src/${src}.cpp -o $(echo $src | tr / _).o
done
clang++ -stdlib=libc++ -shared -fPIC -O2 -o libdiagtxt_libcxx.so \
  *.o -L $BOOST_DIR/lib \
  -Wl,--start-group -lboost_regex -lboost_filesystem -lboost_locale \
  -Wl,--end-group
```

Result: `libdiagtxt_libcxx.so`, ~1 MB, dynamically linked against
system `libc++.so.1`/`libc++abi.so.1`, with all 19 in-scope D16
symbols exported under the `_ZN6zhinst...NSt3__1...` (libc++)
mangling — the same mangling as in the original `_seqc_compiler.so`.

This proves the unified-ABI plan is viable.  Next: fold the loose
shell commands into a proper CMakeLists driven from
`reconstructed/build-libcxx-test/`, parameterised on the Conan-
provided Boost path.

### Build target — libc++ recon test .so

Goal: a minimal shared library, separate from the main recon
`_seqc_compiler.so`, that contains the 19 in-scope D16 symbols
(plus their transitive dependencies) compiled with
`clang++ -stdlib=libc++` so its `std::string` ABI matches the
original `_seqc_compiler.so`.

Build dir: `reconstructed/build-libcxx-test/` (separate from
`reconstructed/build/`).

Boost: must also be libc++ to avoid mixed-ABI link errors at the
`boost::regex` / `boost::filesystem` boundaries.  Built via Conan
profile `clang20-libcxx`, options pinned to `shared=False` so the
resulting test .so is self-contained:

```
conan install --requires=boost/1.91.0 \
  -pr:h clang20-libcxx -pr:b clang20-libcxx \
  -s build_type=Release \
  -o 'boost/*:shared=False' \
  -o 'boost/*:without_python=True' -o 'boost/*:without_test=True' \
  -o 'boost/*:without_graph=True' -o 'boost/*:without_graph_parallel=True' \
  -o 'boost/*:without_mpi=True' -o 'boost/*:without_wave=True' \
  -o 'boost/*:without_log=True' -o 'boost/*:without_stacktrace=True' \
  --build=missing
```

Recon TUs that need to compile under libc++ (transitive closure of
`core/diagnostics_text.cpp`):

- `core/diagnostics_text.cpp` itself
- `core/exception.cpp` (used via `BOOST_THROW_EXCEPTION`)
- `core/platform.cpp` (canonical `quote`)
- `device/device_type.cpp` (used by `generateSfc` / `FeaturesCode`)
- ...others as the link surfaces them

Will discover the actual minimum set by attempting the build and
patching as needed.

### Harness — Python ctypes

- Locate text-segment base of `_seqc_compiler.so` after `dlopen`
  (parse `/proc/self/maps`, or `dlinfo(handle, RTLD_DI_LMID, ...)`
  + ELF program-header scan).
- For each target: original side calls
  `(text_base + offset)`; recon side calls `dlsym(handle,
  mangled)`.
- Construct libc++ `std::string` (24 B SSO; bit 0 of byte 0 = long
  flag) on the Python side.  Helpers:
  - `make_short(s)` for inputs ≤ 22 bytes
  - `make_long(s, alloc)` for longer inputs (must allocate via
    libc++ allocator — but since both sides will be pointed at
    the **same** allocation and we control destruction, raw
    `malloc` should work as long as the function never reallocates
    mid-call.  For functions that mutate — `xmlUnescape`,
    `escapeStringForJson`, etc. — this is risky.  Safer: pre-size
    the buffer to a generous max and pass long-form for everything,
    or call once to find required size, then again with sufficient
    capacity.  TBD — first attempt: long-form with generous
    capacity for in-place mutators).
- Compare results byte-for-byte.

### Open design questions

1. `std::string&` mutators that may reallocate: how do we hand them
   a buffer they can grow safely?  Two options:
   a. Always long-form, with capacity = input_len * 8 + 64; if a
      mutator needs more, the Python side detects the relocation
      attempt by trapping `operator new` (hard).
   b. Construct the string by calling `string::string(const char*)`
      via ctypes (e.g. dlsym a known constructor symbol from a
      libc++.a we pull in, or write a tiny C++ helper TU built
      with libc++ that exposes `extern "C"` makers and freers).
      Cleaner, more robust.  Likely option (b).

2. `dlopen`-ing both .so files in the same Python process: does
   the original (statically linked libc++) and the libc++ test
   .so coexist?  They both have inlined libc++ symbols at
   different addresses, but they're hidden in each .so's local
   scope — should be fine with `RTLD_LOCAL`.  Verify experimentally.
