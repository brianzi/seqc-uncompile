# diagnostics_text cluster — disassembly notes

D16 reconstruction prep.  19 symbols, ~33,844 B total, all in `zhinst::`
namespace.  Zero in-recon callers; downstream-tooling-only surface.
Reconstruction policy: full reconstruction with `\verifyme` per
user decision (2026-05-12).

## Conventions used in this file

- **Address**: VMA in `_seqc_compiler.so`.  Section `.text` lives at
  VMA `0x0cd000` with file offset `0x0cd000` (no PIE shift); `.rodata`
  at VMA `0x8fc400`.
- **String evidence**: literal byte sequences observed in `.rodata`
  pointed-to from the function body (via `lea` against `%rip`).
- **Body sketch**: high-level pseudo-C reconstructing the algorithm.
  When a string literal or regex pattern is present, treat it as
  primary ground truth.
- **Caller graph**: only intra-cluster edges noted; entire cluster has
  zero callers from outside the cluster (verified in D14 inventory).

## Target file layout

- Header: `reconstructed/include/zhinst/core/diagnostics_text.hpp`
- Impl:   `reconstructed/src/core/diagnostics_text.cpp`

All public functions live in `namespace zhinst {}` at file scope.
Anonymous-namespace helpers (none observed in cluster surface) go in
the .cpp.

---

## Per-symbol notes

(Subagent fills in below.  Order: descending by size, with caller
pairs co-located.)

---

### xmlEscapeUtf8Critical(string&)

- **Address**: `0x2faaa0`, size 803 B
- **Mangled**: `_ZN6zhinst21xmlEscapeUtf8CriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `void zhinst::xmlEscapeUtf8Critical(std::string& s)` —
  in-place transform.  rdi = `&s`.

**.rodata strings referenced**

| VMA | Bytes (C-escaped) | Used at |
|-----|-------------------|---------|
| `0x90d165` | `"&#%03d;"` (8 B incl. NUL) | `lea 0x6125f7(%rip),%rsi` @ `2fab67`, passed to `boost::basic_format::basic_format(char const*)` |

No other string literals; the high-byte branch builds `&#NNN;` via boost::format.
The `cmp $0x2c,%eax` at `2fab38` looks like a fast-path check for
SSO buffer header byte (`%al = (size<<1) | 0`, so `0x2c = 22<<1 | 0` →
"local string is exactly at SSO max with no extra space"), not a char
test.  It steers between the SSO append-by-byte fast path
(`2facb8` onwards: `s[size++] = c; s[size] = 0`) and the long-string
realloc/grow path (`2fac10` onwards: `__throw_length_error`,
`operator new`, memmove).

**External calls**

- `boost::basic_format::basic_format(char const*)` — construct format from `"&#%03d;"`
- `boost::basic_format & boost::io::detail::feed_impl<...,put_holder<...> const&>(boost::basic_format&, put_holder const&)` — feed the int argument
- `boost::basic_format::str() const` → `std::string` (NRVO into local at `rbp-0x70`)
- `boost::basic_format::~basic_format()` — destroy the format object every loop iteration
- `std::string::append(char const*, unsigned long)` — concat formatted "&#NNN;" onto the local result string at `rbp-0x40`
- `std::string::__assign_no_alias<true|false>(char const*, unsigned long)` — write result back into `*s`
- `std::string::__throw_length_error[abi:ne200100]()` — for the SSO-grow path overflow check
- `operator new(unsigned long)` / `operator delete(void*, unsigned long)`
- `memmove@plt` — for SSO→long copy on grow
- `_Unwind_Resume@plt` — landing pad for boost::format destructor

**Body sketch**

```cpp
void xmlEscapeUtf8Critical(std::string& s) {
    std::string out;                        // local at rbp-0x40, initially empty SSO
    const char* data = s.data();
    size_t       n   = s.size();
    for (size_t i = 0; i < n; ++i) {
        signed char c = static_cast<signed char>(data[i]);
        if (c >= 0) {                       // ASCII fast path
            out.push_back(static_cast<char>(c));   // SSO-aware in-place grow
        } else {                            // high byte (>=0x80 in unsigned view)
            boost::format f("&#%03d;");
            f % static_cast<int>(c);        // signed int → boost prints negative!
            std::string entity = f.str();   // e.g. "&#-61;" for 0xC3 -- see note
            out.append(entity);
            // entity destroyed; format destroyed
        }
    }
    s = std::move(out);                     // assign back via __assign_no_alias
}
```

**\verifyme — sign-extension subtlety.**  The byte is loaded with
`movsbl (%rax,%r14,1),%eax` (sign-extend), and the `js` test at
`2fab2a` branches on the high bit.  Then the value passed to
boost::format is the *signed* int — so 0xC3 becomes `-61` and the
emitted entity is `"&#-061;"` not the well-formed `"&#195;"`.  This is
either a bug in the original or "Critical" naming hints these are
diagnostics never meant to be valid XML.  GDB-trace before claiming
either way; either behaviour is faithfully reconstructible.

**Layout notes**

- `out` is `std::string` at `rbp-0x40` (24 B libc++ SSO, zeroed at entry)
- `boost::format` object at `rbp-0x168` (size ~0xc8 = 200 B)
- `entity` (boost::format::str() return) at `rbp-0x70`
- loop counter `i` in `r14`; `n` cached at `rbp-0x80`; `data` at `rbp-0x88`

---

### xmlEscapeCritical(string&)

- **Address**: `0x2fa7e0`, size 689 B
- **Mangled**: `_ZN6zhinst17xmlEscapeCriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `void zhinst::xmlEscapeCritical(std::string& s)` —
  in-place transform.  rdi = `&s`.

**.rodata strings referenced**

| VMA | Bytes | Use |
|-----|-------|-----|
| `0x90d027` | `"&(?![gl]t;\|amp;\|quot;\|#[0-9]+;\|#x[0-9a-fA-F]+;)"` (NUL-term) | regex pattern at `2faa1d` — passed to `boost::basic_regex<char,...>::basic_regex(char const*, unsigned int)` |
| `0x90cedc` | `"&lt;"` (5 B incl. NUL, occupies bytes `26 6c 74 3b 00`) | replacement passed to `find_format_all_impl2` at `2fa8eb` |
| `0x90cee7` | `"&gt;"` (5 B) | replacement passed to `find_format_all_impl2` at `2fa98b` |
| `0x900e5b` | single byte `'<'` (`0x3c`) | needle for find-`<` loop |
| `0x900e5c` | single byte `'<'+1`?  Reads as `'>'` at `0x900e5d` → see below | end-of-needle iterator |
| `0x900e5d` | single byte `'>'` (`0x3e`) | needle for find-`>` loop |
| `0x900e5e` | single byte `'A'` (`0x41`) | end-of-needle iterator for `>` (just one byte past) |

Inline `cmpb $0x3c,(%rcx)` at `2fa8a0` and `cmpb $0x3e,(%rcx)` at
`2fa940` are open-coded `std::find(c == '<')` and `std::find(c == '>')`
loops that compute the start of the first match before calling the
boost replace helper.

**Function-local static**: `regex` at `b852f0` with guard at `b85300`,
constructed lazily via `__cxa_guard_acquire/release` and registered
with `__cxa_atexit`.  This is the canonical Meyers singleton compiled
form (`static const boost::regex regex(pattern, 0)`).

**External calls**

- `__cxa_guard_acquire/release/abort` — for the function-local static `regex`
- `boost::basic_regex<...>::basic_regex(char const*, unsigned int)` — flags = 0
- `__cxa_atexit` — register `~basic_regex` with `__dso_handle`
- `boost::regex_replace<string_out_iterator<string>, __wrap_iter<char const*>, regex_traits<char>, char, char const*>(out_it, begin, end, regex, fmt, _match_flags)` —
  this is the **first** pass: it replaces each `&` not already part of
  an entity with `&amp;` (the `fmt` argument at `2fa82f` is
  `0x90ce9b` → `"&amp;"`).  `flags=0` (`xor %r9d,%r9d`).  Result
  written into local `std::string` at `rbp-0x30`.
- `boost::algorithm::detail::find_format_all_impl2<string, first_finderF<char const*, is_equal>, const_formatF<iterator_range<char const*>>, iterator_range<__wrap_iter<char*>>, iterator_range<char const*>>(string&, finder, formatter, search_range, format_range)` —
  generic in-place find/replace.  Called twice:
    - `2fa8eb`: replace all `<` with `&lt;`
    - `2fa98b`: replace all `>` with `&gt;`
  The hand-written `cmp '<'`/`cmp '>'` loops above the calls compute
  the first match position used to seed the search range.
- `std::string::__assign_no_alias<true|false>` — write result back
- `operator delete(void*, unsigned long)` for SSO teardown
- `_Unwind_Resume@plt`

**Body sketch**

```cpp
void xmlEscapeCritical(std::string& s) {
    static const boost::regex re(
        "&(?![gl]t;|amp;|quot;|#[0-9]+;|#x[0-9a-fA-F]+;)", 0);

    std::string out;                                   // rbp-0x30
    boost::regex_replace(
        std::back_inserter(out),                       // string_out_iterator
        s.begin(), s.end(),
        re,
        "&amp;",
        boost::regex_constants::match_default);        // = 0

    boost::algorithm::replace_all(out, "<", "&lt;");
    boost::algorithm::replace_all(out, ">", "&gt;");

    s = std::move(out);
}
```

**Idempotence**: the negative-lookahead regex skips `&` already
followed by `gt;`, `lt;`, `amp;`, `quot;`, `#NNN;`, or `#xHH;`.  Then
the bare `<`/`>` replacers run on the &-escaped intermediate, never
producing double-escaped output for those entities.  Note `&quot;` is
in the lookahead but *not* in any replacement step — quotes are not
escaped by this function (so existing `&quot;` is preserved but bare
`"` is left alone).  This matches "critical" semantics: only escape
characters that would break XML structure.

---

### generateSfc(string const& devType, string const& options)

- **Address**: `0x2d10b0`, size 788 B
- **Mangled**: `_ZN6zhinst11generateSfcERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_`
- **Signature**: `sfc::FeaturesCode zhinst::generateSfc(std::string const& devType, std::string const& options)`.
  rdi = NRVO return slot for `sfc::FeaturesCode`; rsi = devType; rdx = options.

  Wait — register evidence says rdi = NRVO slot, rsi = devType,
  but the call to `toDeviceFamily(devType)` happens with rdi already
  holding what was rdi at entry.  Re-reading: at entry rdi=devType
  ptr, rsi=options ptr, return is via `mov %rbx,%rax` of the saved
  rdi.  So `sfc::FeaturesCode` is **returned in rax** (single
  pointer-sized value, likely a typedef for a pointer to an
  internal type, or an `enum`/`int`).  rdi=devType, rsi=options.
  The function returns the value of `generateMfSfc(...)` directly
  (`mov %rax,%rbx; ... mov %rbx,%rax; ret`).

  Corrected: `sfc::FeaturesCode generateSfc(std::string const& devType, std::string const& options)`,
  and `sfc::FeaturesCode` is a single pointer-sized scalar (8 B,
  trivial — almost certainly `enum class` or a thin wrapper around a
  pointer / `shared_ptr` head).  See nm probe below.

**.rodata strings referenced**

| VMA | Bytes | Use |
|-----|-------|-----|
| `0x90b736` | `"Request to generate SFC code for an unsupported device family ("` (63 B + NUL) | error msg, `mov $0x3f,%edx` confirms length 63 |
| `0x90b16d` | `").\0"` (3 B incl. NUL, `mov $0x2,%edx`) | trailing `")."` of error msg |
| `0x90b6ac` | `"/builds/labone/labone/device/types/src/device_option.cpp"` | source_location file pointer |
| `0x90b776` | `"sfc::FeaturesCode zhinst::generateSfc(const std::string &, const std::string &)"` | source_location function-name pointer |
| immediate `0x2f0000016c` | `line=0x16c=364`, `column=0x2f=47` | packed source_location line+column field |

The `boost::source_location` struct at `rbp-0xb0` is laid out as
`{const char* file; const char* function; uint32_t line; uint32_t column;}`
matching boost 1.79+.

**External calls**

- `zhinst::toDeviceFamily(std::string const&)` — devType → DeviceFamily enum (in `%eax`, then `%ebx`)
- `zhinst::splitDeviceOptions(std::string const&)` — options string → `std::vector<std::string>` at `rbp-0x218`
- `zhinst::toDeviceOption(std::string const&)` — per-token, in a try/catch
- `zhinst::DeviceOptionSet::insert(zhinst::DeviceOption)` — accumulate into set at `rbp-0x80`
- `__cxa_begin_catch` / `__cxa_end_catch` — swallow exception from `toDeviceOption` (unknown options ignored)
- `zhinst::detail::generateMfSfc(std::string const& devType, zhinst::DeviceOptionSet const&)` — **the only emit path**, taken when `family == 4` (`cmp $0x4,%ebx; jne <error>` at `2d11c0`)
- `zhinst::toString(zhinst::DeviceFamily)` — for the error message
- `std::ostringstream` ctor/dtor; `__put_character_sequence`; `operator<<(ostream&, string const&)`; `ostringstream::str() const &`
- `zhinst::Exception::Exception(std::string)` and `~Exception`
- `boost::throw_exception<zhinst::Exception>(zhinst::Exception const&, boost::source_location const&)` — at `2d1310`, throws unconditionally for any non-MF family
- `DeviceOptionSet::~DeviceOptionSet`, `vector<string>::~vector`, `_Unwind_Resume@plt`
- `operator delete`

**Body sketch**

```cpp
sfc::FeaturesCode generateSfc(std::string const& devType,
                              std::string const& options)
{
    DeviceFamily family = toDeviceFamily(devType);

    std::vector<std::string> tokens = splitDeviceOptions(options);
    DeviceOptionSet optSet;
    for (auto const& tok : tokens) {
        try {
            optSet.insert(toDeviceOption(tok));     // skip unknown tokens
        } catch (...) {
            // swallow
        }
    }
    // tokens destroyed here

    if (family == DeviceFamily::MF /* enum value 4 */) {
        return zhinst::detail::generateMfSfc(devType, optSet);
    }

    // Unsupported family → throw
    std::ostringstream oss;
    oss << "Request to generate SFC code for an unsupported device family (";
    oss << zhinst::toString(family);
    oss << ").";
    boost::source_location loc{
        "/builds/labone/labone/device/types/src/device_option.cpp",
        "sfc::FeaturesCode zhinst::generateSfc(const std::string &, const std::string &)",
        364, 47};
    boost::throw_exception(zhinst::Exception(oss.str()), loc);
}
```

**\verifyme** — the value `0x4` for the MF branch was identified by
`cmp $0x4,%ebx` against the result of `toDeviceFamily`.  The mapping
`DeviceFamily::MF == 4` should be verified against the existing
`reconstructed/` enum (it likely already exists since `toDeviceFamily`
is reconstructed).  If the enum disagrees, fix the magic number, not
the disassembly.

**Note on the file path / function string**: the string at `0x90b6ac`
is the path `device/types/src/device_option.cpp`, NOT
`sfc/...generateSfc.cpp`.  The original function was apparently
defined in `device_option.cpp` even though it is nominally in the
`sfc` subsystem.  This contradicts what one would guess from the
function name and is worth flagging when placing the recon source
file (we should NOT mirror this oddity — place in
`reconstructed/src/core/diagnostics_text.cpp` per the cluster plan).

---

### toCheckedString(char const*)

- **Address**: `0x2f2700`, size 179 B
- **Mangled**: `_ZN6zhinst15toCheckedStringEPKc`
- **Signature**: `std::string zhinst::toCheckedString(char const* p)`.
  Returns by value via NRVO slot in **rdi** (caller-allocated 24 B
  string buffer); the actual `char const*` arg is in **rsi**.  Returns
  rdi in rax.

**.rodata strings referenced**

NONE.  The size (179 B) is entirely accounted for by the libc++
`__init`-equivalent open-coded body: SSO branch, long-string branch
(operator new + memcpy), and `__throw_length_error` tail.

**External calls**

- `strlen@plt`
- `operator new(unsigned long)` — long-string allocation
- `memcpy@plt`
- `std::string::__throw_length_error[abi:ne200100]()` — when `len >= 0xfffffffffffffff8` (i.e. `npos - 8`)

**Body sketch**

```cpp
std::string toCheckedString(char const* p) {
    if (p == nullptr) {
        return std::string{};               // SSO empty: zeroed 24 bytes
    }
    size_t n = std::strlen(p);
    if (n >= std::string::npos - 7)
        std::string::__throw_length_error();

    std::string s;
    if (n < 23) {
        // SSO path: byte0 = (n<<1), data inline at &s+1
        s.__set_short_size(n);
        std::memcpy(s.__short_data(), p, n);
        s.__short_data()[n] = '\0';
    } else {
        size_t cap = (n | 7) + 1;
        if (cap == 24) cap = 26;            // adjustment seen at 2f2766
        char* buf = static_cast<char*>(::operator new(cap));
        s.__set_long_pointer(buf);
        s.__set_long_cap(cap);              // stored as (cap | 1)
        s.__set_long_size(n);
        std::memcpy(buf, p, n);
        buf[n] = '\0';
    }
    return s;
}
```

**Conclusion on "is it more than a wrapper?"**: NO.  It is exactly
`p ? std::string(p) : std::string()` open-coded with libc++ SSO
internals exposed (the compiler inlined the `string(char const*)`
ctor).  No whitespace stripping, no UTF-8 validation, no other
trickery.  The 179 B is just the SSO-vs-long bifurcation plus the
length-overflow guard that every libc++ string ctor carries.  Equivalent
single-line recon:

```cpp
std::string toCheckedString(char const* p) { return p ? std::string(p) : std::string(); }
```

with sufficient `[[gnu::always_inline]]` discipline this should
recompile to the same body.



---

## Subagent pass — 2026-05-12 — six functions analysed

Source: direct disassembly of `_seqc_compiler.so` plus `objdump -s -j
.rodata` for every `.rip`-relative literal observed in the bodies.
Replacement-string lengths verified by reading bytes between the
two `lea` operands (`-0x38`/`-0x30` or `-0x50`/`-0x48` slots — the
4×8-byte `iterator_range` quad pushed onto stack args).

### Common shape used by 4 of the 6 functions

Five functions (`sanitizeFilename`, `escapeStringForJson`,
`escapeStringForPython`, plus a simple search loop) compose
character-class substitutions by repeated calls to:

```
boost::algorithm::detail::find_format_all_impl2<
    std::string,
    boost::algorithm::detail::first_finderF<const char*, boost::algorithm::is_equal>,
    boost::algorithm::detail::const_formatF<boost::iterator_range<const char*>>,
    boost::iterator_range<std::__wrap_iter<char*>>,
    boost::iterator_range<const char*>>
    (string&, finder, formatter, range, range);
```

at file offset `0x2fdd30` (single-char-find / const-format helper).
Each call site:

1. Builds a `(begin, end)` iterator_range pointing at a 1-byte literal
   in `.rodata` (the search character).  Stores it in `-0x50/-0x48`.
2. Builds a `(begin, end)` iterator_range pointing at the replacement
   bytes in `.rodata` (length = `end - begin`).  Stores it in
   `-0x38/-0x30`.
3. Inlines a manual scan over the live string contents to FIND the
   first matching byte (this is just `find_first_of` with a one-byte
   class — gives early-out without entering boost machinery).
4. If the inline scan finds at least one match, calls `impl2` which
   replaces ALL occurrences of the search byte with the replacement
   range and reflows the string in place.
5. Reloads SSO `(flag, size, ptr)` triple from the (possibly
   reallocated) string, and proceeds to the next pass.

Replacement ranges of `length 0` (begin == end) act as a **delete**
— used by `sanitizeFilename` for every disallowed character.

The libc++ short-string short-circuit (when `byte0 & 1 == 0`, data
lives at `string + 1`, length = `byte0 >> 1`) is open-coded at every
pass entry: this is what the `cmove %r14,%rax; cmove %rdx,%r9`
sequences (where `%r14 = string+1`) compute.

---

### `sanitizeFilename`

- **Address**: `0x2fcbe0`, **size**: 2060 B
- **Mangled**: `_ZN6zhinst16sanitizeFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `void zhinst::sanitizeFilename(std::string&)`
- **Calls**: only `find_format_all_impl2` (11×).  No regex.

#### Pass list (in execution order)

| #  | Search target           | Replacement range                  | Effect                  |
|----|--------------------------|------------------------------------|-------------------------|
| 1  | substring `../`         | `[0x8fddf3, 0x8fddf3)` length 0    | delete every `../`      |
| 2  | byte `/` (0x2f)         | `[0x8fddf3, 0x8fddf3)`             | delete every `/`        |
| 3  | substring `..\`         | `[0x8fddf3, 0x8fddf3)`             | delete every `..\`      |
| 4  | byte `\` (0x5c)         | `[0x8fddf3, 0x8fddf3)`             | delete every `\`        |
| 5  | byte `:` (0x3a)         | `[0x8fddf3, 0x8fddf3)`             | delete every `:`        |
| 6  | byte `*` (0x2a)         | `[0x8fddf3, 0x8fddf3)`             | delete every `*`        |
| 7  | byte `?` (0x3f)         | `[0x8fddf3, 0x8fddf3)`             | delete every `?`        |
| 8  | byte `"` (0x22)         | `[0x8fddf3, 0x8fddf3)`             | delete every `"`        |
| 9  | byte `|` (0x7c)         | `[0x8fddf3, 0x8fddf3)`             | delete every `|`        |
| 10 | byte `<` (0x3c)         | `[0x8fddf3, 0x8fddf3)`             | delete every `<`        |
| 11 | byte `>` (0x3e)         | `[0x8fddf3, 0x8fddf3)`             | delete every `>`        |

(All replacement-end pointers equal the begin pointer → empty range
→ deletion semantics in boost's `const_formatF`.)

Disallowed-character set, full list:

```
. . / | / | . . \ | \ | : | * | ? | " | | | < | >
```

i.e.:  the literal three-byte sequences `../` and `..\`, then any
single occurrence of `/  \  :  *  ?  "  |  <  >`.

Note the **single-dot tolerance**: a lone `.` is preserved (only
`..` followed by `/` or `\` is stripped).  Pass 1 / pass 3 each
inline a tiny state machine that walks the byte stream, looks
ahead two bytes, and only invokes `impl2` when the three-byte
prefix matches.

#### `.rodata` literals referenced

| Addr      | Bytes (hex)  | Decoded |
|-----------|--------------|---------|
| `0x8fddf3` | `00`        | empty replacement (zero-length) |
| `0x8fde00` | `2e 2e 2f` (the `../` literal lives in the literal pool around here for finder seeding) | `../` |
| `0x90ce55` | `2f 00`     | `/`     |
| `0x90d0aa` | `2e 2e 2f`  | `../`   |
| `0x90d0ad` | `00`        | terminator |
| `0x90d0ae` | `2e 2e 5c`  | `..\`   |
| `0x900e42` | `5c 00`     | `\`     |
| `0x900e77` | `3a 00`     | `:`     |
| `0x900e36` | `2a 00`     | `*`     |
| `0x900e3a` | `3f 00`     | `?`     |
| `0x8fe951` | `22 00`     | `"`     |
| `0x900e40` | `7c 00`     | `|`     |
| `0x900e5b` | `3c 00`     | `<`     |
| `0x900e5d` | `3e 00`     | `>`     |

(These character constants live in a packed grammar/literal pool
shared with other passes elsewhere in the binary.)

#### Pseudo-C body

```cpp
void sanitizeFilename(std::string& s) {
    // delete dangerous path-traversal sequences
    boost::algorithm::erase_all(s, "../");
    boost::algorithm::erase_all(s, "/");
    boost::algorithm::erase_all(s, "..\\");
    boost::algorithm::erase_all(s, "\\");
    // delete every Windows-illegal filename character
    for (char c : "*?\":|<>") boost::algorithm::erase_all(s, std::string(1,c));
    // (calls actually use find_format_all_impl2 with empty replacement;
    //  semantically equivalent to erase_all)
}
```

---

### `sanitizeInvalidFilename`

- **Address**: `0x2fd3f0`, **size**: 829 B
- **Mangled**: `_ZN6zhinst23sanitizeInvalidFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `void zhinst::sanitizeInvalidFilename(std::string&)`
- **Calls**:
  - `zhinst::sanitizeFilename(string&)` (in-place pre-pass)
  - `boost::filesystem::detail::path_algorithms::stem_v3(path const&)`
  - `boost::filesystem::detail::path_algorithms::extension_v3(path const&)`
  - `boost::filesystem::detail::path_algorithms::remove_filename_v3(path&)`
  - `boost::filesystem::detail::path_algorithms::replace_extension_v3(path&, path const&)`
  - `boost::regex_match<wrap_iter<const char*>, …>` (with `match_default | match_continuous`-flagged value `0x400` = `match_perl`-ish; full constant is `0x400`)
  - libc++ string init/assign helpers (`__init_copy_ctor_external`,
    `__assign_no_alias<true|false>`)

#### Static state

A function-local static `boost::basic_regex<char, …> re` lives at
`0xb85338`, guarded by `0xb85348` via standard `__cxa_guard_acquire/
release` thread-safe init.  Constructed once with:

- pattern: `.rodata` bytes at `0x90d0b2..0x90d0c0` = `COM[1-9]|PRN`
  (12 bytes, NUL-terminated)
- flags: `0x100000` (boost regex `no_except`)

The regex is registered for at-exit destruction via `__cxa_atexit`
with `~basic_regex`.

#### `.rodata` literals

| Addr       | Bytes              | Meaning                       |
|------------|--------------------|-------------------------------|
| `0x90d0b2` | `COM[1-9]\|PRN\0`  | static reserved-name regex    |

#### Body sketch

```cpp
void sanitizeInvalidFilename(std::string& s) {
    sanitizeFilename(s);                    // strip path-traversal + illegal chars first
    boost::filesystem::path p(s);           // implicit; via stem_v3
    auto stem = p.stem();                   // separate stem and extension
    static const boost::regex re("COM[1-9]|PRN", boost::regex::no_except);
    if (boost::regex_match(stem.string(), re)) {
        // stem matches a Windows reserved name → mangle by stripping it
        auto ext = p.extension();
        p.remove_filename();
        p.replace_extension(ext);           // odd: drops stem entirely, keeps only path+ext
        s = p.string();
    }
    // else: leave sanitized string unchanged
}
```

\verifyme — the `replace_extension` call after `remove_filename` is
unusual (stem disappears entirely).  GDB trace recommended on a known
"PRN.txt" input to confirm the output is just `.txt` (or the parent
directory + `.txt`).

\verifyme — the regex `COM[1-9]|PRN` is missing `LPT[1-9]`, `AUX`,
`CON`, `NUL`, `COM0` and the `^...$` anchors that Windows itself
requires; this is documented as-is from the binary, including the
suspicious incompleteness.

---

### `escapeStringForJson`

- **Address**: `0x2f89b0`, **size**: 1663 B
- **Mangled**: `_ZN6zhinst19escapeStringForJsonERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `void zhinst::escapeStringForJson(std::string&)`
- **Note on signature**: takes a `string&` reference; returns void
  (mutates in place).  An internal scratch copy is taken on entry
  (`__init_copy_ctor_external`), the work is done on the copy, then
  the result is assigned back to `*this` at function exit
  (`__assign_no_alias<true|false>`).  This shields the user string
  from intermediate growth/realloc churn during 7+ passes.
- **Calls**: `find_format_all_impl2` (8×), `boost::basic_regex::do_assign`,
  `boost::algorithm::replace_all_regex<...>`, libc++ string ops.

#### Escape table (in execution order)

| #  | Search byte / pattern                                | Replacement (bytes, decoded) | Notes                         |
|----|-------------------------------------------------------|------------------------------|-------------------------------|
| 1  | `\` (0x5c)                                           | `90cfd4..90cfd6` = `\\` (2)  | escape backslash first        |
| 2  | `"` (0x22)                                           | `90cfd7..90cfd9` = `\"` (2)  | escape double-quote           |
| 3  | LF `\n` (0x0a)                                       | `90cfda..90cfdc` = `\n` (2)  | escape newline (literal)      |
| 4  | BS `\b` (0x08)                                       | `90cfdd..90cfdf` = `\b` (2)  | escape backspace              |
| 5  | FF `\f` (0x0c)                                       | `90cfe0..90cfe2` = `\f` (2)  | escape form-feed              |
| 6  | CR `\r` (0x0d)                                       | `90cfe3..90cfe5` = `\r` (2)  | escape carriage-return        |
| 7  | TAB `\t` (0x09)                                      | `90cfe6..90cfe8` = `\t` (2)  | escape tab                    |
| 8  | regex `&((#0*34)|(#x0*22)|(quot));` (`90cfed..90d008`) | format string `\\$&` (4 bytes, SSO), boost `replace_all_regex` | escape any pre-existing XML-encoded quote with a backslash prefix |
| 9  | `"` (0x22) again                                     | `90d009..90d010` = `&#0034;` (7) | any remaining `"` → XML decimal entity (\verifyme — under what conditions a `"` survives passes 2 and 8 is unclear; possibly defence-in-depth) |

#### `.rodata` literal pool (single shared block at 0x90cfd4–0x90d010)

```
0x90cfd4: 5c 5c 00 5c 22 00 5c 6e 00 08 00 5c 62 00 0c 00
0x90cfe4: 5c 66 00 5c 72 00 5c 74 00 26 28 28 23 30 2a 33
0x90cff4: 34 29 7c 28 23 78 30 2a 32 32 29 7c 28 71 75 6f
0x90d004: 74 29 29 3b 00 26 23 30 30 33 34 3b 00
```

Decoded: `\\\0\"\0\n\0\b\0\f\0\r\0\t\0&((#0*34)|(#x0*22)|(quot));\0&#0034;\0`

#### Body sketch

```cpp
void escapeStringForJson(std::string& s) {
    std::string tmp = s;                     // work on a private copy
    boost::algorithm::replace_all(tmp, "\\", "\\\\");
    boost::algorithm::replace_all(tmp, "\"", "\\\"");
    boost::algorithm::replace_all(tmp, "\n", "\\n");
    boost::algorithm::replace_all(tmp, "\b", "\\b");
    boost::algorithm::replace_all(tmp, "\f", "\\f");
    boost::algorithm::replace_all(tmp, "\r", "\\r");
    boost::algorithm::replace_all(tmp, "\t", "\\t");
    static const boost::regex xml_quote("&((#0*34)|(#x0*22)|(quot));",
                                        boost::regex::no_except);
    boost::algorithm::replace_all_regex(tmp, xml_quote, std::string("\\\\$&"));
    boost::algorithm::replace_all(tmp, "\"", "&#0034;");
    s = std::move(tmp);
}
```

\verifyme on the trailing `replace_all(tmp, "\"", "&#0034;")` —
candidate explanation: pass 8's `\\$&` only adds a backslash in
front of `&...;` sequences, not a JSON-escape; some downstream
consumer evidently wants those quote-entities to round-trip as
`\&#0034;` AND any literal `"` introduced by the regex format-string
expansion to land as the canonical numeric entity.  GDB trace on
input containing `&quot;` would confirm.

---

### `escapeStringForPython`

- **Address**: `0x2f9780`, **size**: 1644 B
- **Mangled**: `_ZN6zhinst21escapeStringForPythonENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string zhinst::escapeStringForPython(std::string s)` — by-value parameter, return-via-`%rdi` NRVO slot.  At entry: `%rdi = return slot`, `%rsi = &param string`.  At exit: param string is move-emptied into the return slot (final block at `2f9dc1..2f9deb` copies the SSO triple into `*%r14` and zeros the source).
- **Calls**: `find_format_all_impl2` only (10×).  No regex.

#### Escape table (in execution order)

| #  | Search byte             | Replacement (bytes, decoded)        | Effect                |
|----|--------------------------|-------------------------------------|-----------------------|
| 1  | `\` (0x5c)              | `90cfd4..90cfd6` = `\\` (2)         | escape backslash      |
| 2  | `'` (0x27)              | `90d021..90d023` = `\'` (2)         | escape single-quote   |
| 3  | `"` (0x22)              | `90cfd7..90cfd9` = `\"` (2)         | escape double-quote   |
| 4  | BEL `\a` (0x07)         | `90d019..90d01b` = `\a` (2)         | escape bell           |
| 5  | BS `\b` (0x08)          | `90cfdd..90cfdf` = `\b` (2)         | escape backspace      |
| 6  | FF `\f` (0x0c)          | `90cfe0..90cfe2` = `\f` (2)         | escape form-feed      |
| 7  | LF `\n` (0x0a)          | `90cfda..90cfdc` = `\n` (2)         | escape newline        |
| 8  | CR `\r` (0x0d)          | `90cfe3..90cfe5` = `\r` (2)         | escape carriage-ret   |
| 9  | TAB `\t` (0x09)         | `90cfe6..90cfe8` = `\t` (2)         | escape tab            |
| 10 | VT `\v` (0x0b)          | `90d01e..90d020` = `\v` (2)         | escape vertical-tab   |

**No `\0`/NUL escape, no generic `\x..` hex pass, no `\u`/`\U`** —
contrary to the task hypothesis.  Only the ten Python literal
escape forms above are emitted.

#### `.rodata` (additional Python-only block at 0x90d011–0x90d026)

```
0x90d010: 00 27 27 00 25 25 00 07 00 5c 61 00 0b 00 5c 76
0x90d020: 00 5c 27 00 5c 30 00
```

Of these only `\a` (`5c 61`), `\v` (`5c 76`), `\'` (`5c 27`) are
used by escapeStringForPython.  The leading `''`, `%%`, raw `\a`,
raw `\v`, and `\0` bytes are search-character constants for other
passes (the search target lives at e.g. `90d017 = 0x07` for pass 4).

#### Body sketch

```cpp
std::string escapeStringForPython(std::string s) {
    boost::algorithm::replace_all(s, "\\",   "\\\\");
    boost::algorithm::replace_all(s, "'",    "\\'");
    boost::algorithm::replace_all(s, "\"",   "\\\"");
    boost::algorithm::replace_all(s, "\a",   "\\a");
    boost::algorithm::replace_all(s, "\b",   "\\b");
    boost::algorithm::replace_all(s, "\f",   "\\f");
    boost::algorithm::replace_all(s, "\n",   "\\n");
    boost::algorithm::replace_all(s, "\r",   "\\r");
    boost::algorithm::replace_all(s, "\t",   "\\t");
    boost::algorithm::replace_all(s, "\v",   "\\v");
    return s;
}
```

\verifyme — by-value parameter is unusual given JSON/Filename siblings
take `string&`.  This lets the function re-use the caller's storage
as the working buffer.  Confirmed by the `mov %rsi,%rbx` at entry
followed by the move-empty epilogue (`xorps %xmm0,%xmm0; movups %xmm0,(%rbx); movq $0x0,0x10(%rbx)`).

---

### `truncateXmlSafe`

- **Address**: `0x2fc690`, **size**: 817 B
- **Mangled**: `_ZN6zhinst15truncateXmlSafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm`
- **Signature**: `void zhinst::truncateXmlSafe(std::string& s, std::size_t maxBytes)`
- **Calls**:
  - `boost::regex_search<wrap_iter<const char*>, …>` (file 0x306a80)
  - `zhinst::truncateUtf8Safe(string&, ulong)` (fallback path)
  - `std::string::__erase_external_with_move(pos, n)`
  - `std::__shared_weak_count::__release_weak()` (match_results dtor)
  - `std::logic_error::logic_error(const char*)` and
    `boost::throw_exception<std::logic_error>` for an
    "uninitialized boost::match_results" guard

#### Static state

Function-local static `boost::basic_regex<char,…> regex` at
`0xb85320`, guarded by `0xb85330` (`__cxa_guard_acquire/release`,
`__cxa_atexit`-registered destructor).  Constructed once with:

- pattern: `.rodata` bytes at `0x90d072..0x90d0aa` =
  `&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;` (56 bytes,
  NUL-terminated)
  — note the `&gt` without trailing `;` is present in the binary
  exactly as written (intentional per task spec; treat as authoritative).
- flags: `0` (default boost::regex options)

#### `.rodata` literals

| Addr       | Bytes                                                 | Meaning             |
|------------|-------------------------------------------------------|---------------------|
| `0x90d072` | `&#x[0-9a-fA-F]+;\|&#[0-9]+;\|&amp;\|&lt;\|&gt\|&quot;\|&apos;\0` | XML entity regex |
| `0x901eed` | `Attempt to access an uninitialized boost::match_res…` | logic_error msg    |

#### Algorithm (verified from disassembly + GDB on the original)

GDB-confirmed against `_seqc_compiler.so` on
`("abc&amp;def", 5)`: the original returns `"abc"` — i.e. the entire
`&amp;def` tail is erased because the entity's end iterator extends
past `data+5`.  See IF-265 in `incidental_findings.md` for the trace.

1. If `s.size() <= maxBytes` → return immediately (no truncation).
2. If `maxBytes == 0` → empty the string in place and return.
3. Walk **backwards** from `data() + maxBytes` looking for the most
   recent `&` in `[data, data+maxBytes)` (loop at
   `2fc700..2fc712`).  Let `entityCandidate` be the position of
   that `&` (NOT one past it), or `data` if no `&` was found.  This
   becomes the **search START** for the regex — the search range is
   `[entityCandidate, end)` over the full live string.  The
   back-up exists so that a partial entity straddling the cut is
   the first thing the regex sees.
4. Run `boost::regex_search` on `[entityCandidate, end)` with
   `match_default = 0` flags.
5. If the search succeeds **and** the matched entity's end iterator
   `m[0].second` is past `data + maxBytes`, the entity straddles
   the cut: erase from the entity's `&` (`m[0].first - data`)
   through end-of-string with `__erase_external_with_move`.
6. Otherwise (no match, or the match ends at or before the cut):
   fall through to `truncateUtf8Safe(s, maxBytes)` for a clean
   UTF-8 boundary cut.
7. Drop the temporary `match_results` (release the shared regex
   implementation refcount).

The pattern has **no capture groups** — every alternative is a
literal or character class.  `m[1..]` are therefore always empty
sub_matches; only `m[0]` (the whole match) carries useful
iterators.  An earlier reconstruction misread the
`lea -0x68(%rbp),%r8 / cmovge %rdi,%r8` block as selecting between
`m[0]` and `m[3]`; it actually selects between `m[0].first` and
`m[0].second` based on the un-init-access guard.  See IF-265.

A `std::logic_error("Attempt to access an uninitialized
boost::match_results")` is thrown in the un-init-access guard at
`2fc7a5` and `2fc953`.

#### Body sketch

```cpp
void truncateXmlSafe(std::string& s, std::size_t maxBytes) {
    if (s.size() <= maxBytes) return;
    if (maxBytes == 0) { s.clear(); return; }

    const char* data = s.data();
    const char* end  = data + s.size();
    const char* cut  = data + maxBytes;

    // Walk backwards from `cut` to the most recent '&'.  This is the
    // search START — passed as the first iterator to regex_search so
    // the engine only sees entities that begin at or before the cut.
    const char* entityCandidate = data;
    for (const char* p = cut; p > data; --p) {
        if (p[-1] == '&') { entityCandidate = p - 1; break; }
    }
    static const boost::regex regex(
        "&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;");
    boost::cmatch m;
    if (boost::regex_search(entityCandidate, end, m, regex,
                            boost::regex_constants::match_default)
        && m[0].second > cut) {
        s.erase(static_cast<std::size_t>(m[0].first - data),
                std::string::npos);
        return;
    }
    truncateUtf8Safe(s, maxBytes);
}
```

---

### `truncateUtf8Safe`

- **Address**: `0x2fca40`, **size**: 337 B
- **Mangled**: `_ZN6zhinst16truncateUtf8SafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm`
- **Signature**: `void zhinst::truncateUtf8Safe(std::string& s, std::size_t maxBytes)`
- **Calls**: `std::string::__erase_external_with_move(pos, n)` and
  `std::string::erase[abi:ne200100](wrap_iter, wrap_iter)`.  No
  regex, no allocations, no other helpers.

#### `.rodata` literals

None.  All constants (`0xc0`, `0xe0`, `0xf0`, mask `0xf8/0xf0/0xe0`)
are immediates.

#### Algorithm — verified from disassembly

1. If `maxBytes == 0` → empty the string and return (handles long &
   short SSO cases at `2fca6b..2fca7a`).
2. If `s.size() <= maxBytes` → return (no truncation).
3. Let `cut = data + maxBytes`.
4. Inspect the byte AT `cut` (the first byte that would be removed):
   if `(*cut & 0xff) > 0xbf`, i.e. it is **not** a UTF-8 continuation
   byte (continuations are `0x80..0xbf`), the cut is already at a
   codepoint boundary — fall through to step 7.
5. Otherwise walk **backwards** from `cut` looking for the leading
   byte of the partial codepoint (any byte with value `>= 0xc0`).
   Tracked as `bx` (signed offset, decremented each step).  Stops
   when `bx + maxBytes == 0` (start of buffer) or a leading byte is
   found.
6. Inspect the leading byte's high bits to determine the codepoint's
   byte-length:
   - `(b & 0xe0) == 0xc0` → 2-byte codepoint → `nbytes = 2`
   - `(b & 0xf0) == 0xe0` → 3-byte codepoint → `nbytes = 3`
   - `(b & 0xf8) == 0xf0` → 4-byte codepoint → `nbytes = 4`
   The arithmetic at `2fcb3f..2fcb6c` produces this `nbytes`
   (`cmov` chain over the three classes).

   If the codepoint we partially included has length `nbytes` and
   the number of bytes already-included for it (`-bx + 1`) is
   **less than** `nbytes`, advance `cut` to the START of that
   codepoint (drop the partial sequence).  Otherwise the codepoint
   was fully included; keep `cut` where it is.

7. Tail-call `s.erase(cut, end)` — physically the function
   `pop`s its callee-saved registers and `jmp`s straight to
   `__erase_external_with_move` (or `erase(iter, iter)` for the
   partial-codepoint branch).  The string is shortened to end at
   `cut`.

The branch selecting the erase variant:
- `2fcafd..2fcb05`: `pop` + `jmp ecad0` (`__erase_external_with_move(pos, n)`)
  — used when no partial-codepoint trimming is needed.
- `2fcb84..2fcb8c`: `pop` + `jmp 2fc9d0`
  (`std::string::erase(wrap_iter, wrap_iter)`) — used when the
  partial-codepoint adjustment moved `cut` to the start of a
  codepoint and we need to erase by-iterator.

#### Body sketch

```cpp
void truncateUtf8Safe(std::string& s, std::size_t maxBytes) {
    if (maxBytes == 0)         { s.clear(); return; }
    if (s.size() <= maxBytes)  return;

    char*  data = s.data();
    char*  cut  = data + maxBytes;
    char*  end  = data + s.size();

    // Fast path: cut already lands on a codepoint boundary.
    if ((unsigned char)*cut <= 0xbf) {
        // *cut is a continuation byte → walk back to the leading byte.
        std::ptrdiff_t back = 0;
        unsigned char  lead = 0;
        for (std::ptrdiff_t i = 0; i < (std::ptrdiff_t)maxBytes; ++i) {
            unsigned char b = (unsigned char)cut[-1 - i];
            if (b >= 0xc0) { lead = b; back = i + 1; break; }
        }
        if (lead) {
            std::size_t nbytes =
                ((lead & 0xe0) == 0xc0) ? 2 :
                ((lead & 0xf0) == 0xe0) ? 3 :
                ((lead & 0xf8) == 0xf0) ? 4 : 1;
            // bytes-of-codepoint already in [data, cut) is `back`.
            // If we have a partial codepoint, push cut back to its start.
            if (nbytes > back)
                cut -= back;
        }
    }
    s.erase(cut - data, std::string::npos);
}
```

\verifyme — boundary detection deduced from the `cmpb $0xbf`
short-circuit and the `0xc0/0xe0/0xf0` mask chain.  GDB trace
on a 2-/3-/4-byte UTF-8 input cut mid-codepoint would confirm
the precise drop-vs-keep policy.


### linkToQuery

- **Address**: `0x2f2f20`, size 4365B
- **Mangled**: `_ZN6zhinst11linkToQueryERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string zhinst::linkToQuery(std::string const& s)`

**Algorithm**

URL-percent-encodes a fixed set of 27 characters in `s` by repeatedly
calling a single boost::algorithm helper, once per character.  Returns
the encoded copy in `*rdi` (NRVO, sret).  The function does **not**
use `boost::regex`.  Instead each pass is:

1. Manually scan the string for the next occurrence of needle byte
   `c` (an open-coded `memchr`-style loop, inlined per character).
2. If found, dispatch to the same templated boost helper:

   ```
   void boost::algorithm::detail::find_format_all_impl2<
       std::string,
       boost::algorithm::detail::first_finderF<char const*, boost::algorithm::is_equal>,
       boost::algorithm::detail::const_formatF<boost::iterator_range<char const*> >,
       boost::iterator_range<std::__1::__wrap_iter<char*> >,
       boost::iterator_range<char const*> >(...)
   ```
   at `0x2fdd30`.  This is the equivalent of
   `boost::algorithm::replace_all(s, needle, replacement)` with the
   needle already located.

3. Re-read the string layout after each call (the boost helper may
   reallocate) and proceed to the next character.

The `%` (0x25) replacement is intentionally placed **after** the LF/
TAB/CR/SP/`"`/`#`/`$` block but **before** `&`, so that `%` itself is
encoded as `%25` early enough that subsequent `%XX` escapes inserted
by later passes are not double-encoded.

**Static character-encoding table** (verified against `cmpb` ops in
disassembly and `.rodata` at 0x90ce20 + 0x8ff19b/0x906c59/0x8ffd43/
0x900e44):

| pass | needle | replacement | needle .rodata | replacement .rodata |
|------|--------|-------------|----------------|---------------------|
|  1 | `0x0A` LF  | `"%0A"` | 0x8ff19b | 0x90ce25 |
|  2 | `0x09` TAB | `"%09"` | 0x906c59 | 0x90ce29 |
|  3 | `0x0D` CR  | `"%0D"` | 0x8ffd43 | 0x90ce2d |
|  4 | `0x20` SP  | `"%20"` | 0x904dda | 0x90ce31 |
|  5 | `0x22` `"` | `"%22"` | 0x8fe951 | 0x90ce35 |
|  6 | `0x23` `#` | `"%23"` | 0x900e44 | 0x90ce39 |
|  7 | `0x24` `$` | `"%24"` | 0x900e32 | 0x90ce3d |
|  8 | `0x25` `%` | `"%25"` | 0x900e36 | 0x90ce43 |
|  9 | `0x26` `&` | `"%26"` | 0x900e38 | 0x90ce49 |
| 10 | `0x2B` `+` | `"%2B"` | 0x900e3a | 0x90ce4d |
| 11 | `0x2C` `,` | `"%2C"` | 0x900e3c | 0x90ce51 |
| 12 | `0x2F` `/` | `"%2F"` | 0x900e3e | 0x90ce57 |
| 13 | `0x3A` `:` | `"%3A"` | 0x900e40 | 0x90ce5b |
| 14 | `0x3B` `;` | `"%3B"` | 0x900e42 | 0x90ce5f |
| 15 | `0x3C` `<` | `"%3C"` | 0x900e44 | 0x90ce63 |
| 16 | `0x3D` `=` | `"%3D"` | 0x900e46 | 0x90ce67 |
| 17 | `0x3E` `>` | `"%3E"` | 0x900e48 | 0x90ce6b |
| 18 | `0x3F` `?` | `"%3F"` | 0x900e3a | 0x90ce6f |
| 19 | `0x40` `@` | `"%40"` | 0x900e3c | 0x90ce75 |
| 20 | `0x5B` `[` | `"%5B"` | 0x900e3e | 0x90ce79 |
| 21 | `0x5C` `\` | `"%5C"` | 0x900e42 | 0x90ce7d |
| 22 | `0x5D` `]` | `"%5D"` | 0x900e34 | 0x90ce81 |
| 23 | `0x5E` `^` | `"%5E"` | 0x900e36 | 0x90ce85 |
| 24 | `0x60` `` ` `` | `"%60"` | 0x900e3a | 0x90ce8b |
| 25 | `0x7B` `{` | `"%7B"` | 0x900e48 | 0x90ce8f |
| 26 | `0x7C` `\|` | `"%7C"` | 0x900e40 | 0x90ce93 |
| 27 | `0x7D` `}` | `"%7D"` | 0x900e4a | 0x90ce97 |

(The 1-byte needle .rodata addresses are arbitrary single-byte
literals re-used from elsewhere in `.rodata`; they simply provide a
`(begin,end)` `iterator_range` to the boost helper.)

**Strings near but NOT used by linkToQuery**:
- `"&amp;"`  at 0x90ce98
- `"&Omega;"` at 0x90cea0
- `"&#937;"`  at 0x90cea8

These three live in the same rodata block (probably emitted by the
same source file) but the disassembly contains no `lea` to any of
them.  They almost certainly belong to a sibling diagnostics-text
helper (HTML-entity escape) elsewhere in the cluster.

**External calls**: only one — `find_format_all_impl2` at
`0x2fdd30`, called 27 times.  The string copy at entry uses
`std::string::__init_copy_ctor_external` (`0xf14c0`); `operator
delete(void*, ulong)` (`0x8c4e80`) is on the unwind path only.

**Pseudo-C body sketch**

```cpp
std::string zhinst::linkToQuery(std::string const& s) {
    std::string out = s;                          // copy
    static constexpr struct { char c; const char* repl; } kTable[] = {
        {'\n', "%0A"}, {'\t', "%09"}, {'\r', "%0D"},
        {' ',  "%20"}, {'"',  "%22"}, {'#',  "%23"},
        {'$',  "%24"}, {'%',  "%25"}, {'&',  "%26"},
        {'+',  "%2B"}, {',',  "%2C"}, {'/',  "%2F"},
        {':',  "%3A"}, {';',  "%3B"}, {'<',  "%3C"},
        {'=',  "%3D"}, {'>',  "%3E"}, {'?',  "%3F"},
        {'@',  "%40"}, {'[',  "%5B"}, {'\\', "%5C"},
        {']',  "%5D"}, {'^',  "%5E"}, {'`',  "%60"},
        {'{',  "%7B"}, {'|',  "%7C"}, {'}',  "%7D"},
    };
    for (auto [c, repl] : kTable)
        boost::algorithm::replace_all(out,
            std::string(1, c), std::string(repl));
    return out;
}
```

(In the binary the calls are unrolled; the table is implicit in the
emitted code.  A clean reconstruction may keep the table explicit
and loop over it.)

---

### queryToLink

- **Address**: `0x2f4030`, size 596B
- **Mangled**: `_ZN6zhinst11queryToLinkERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string zhinst::queryToLink(std::string const& s)`

**Algorithm**

The inverse of `linkToQuery`: scans for `%` followed by exactly two
hex digits and decodes `%HH` back to the corresponding byte.  Bytes
that are `%` not followed by two hex digits, or any other byte, are
copied verbatim.  Implemented as a hand-rolled state machine, NOT a
loop over a table and NOT a regex — much smaller (596B) than
`linkToQuery`.

Sequence per iteration:

1. `memchr(p, '%', end-p)` to find next `%`.
2. Append `[p, %)` verbatim to `out`.
3. If `end - %pos < 3`, append the rest and stop.
4. Look at the two bytes after `%`; if both are `isxdigit()`
   (verified via `__ctype_b_loc()` mask `0x10` = `_ISxdigit`):
   - Build a 2-byte short-SSO `std::string` containing those two
     hex chars (stack temporary at `-0x50(%rbp)` with
     `(2 << 1) = 0x04` length byte, two payload bytes, and a NUL
     terminator).
   - Call `std::stoi(tmp, nullptr, 16)` (`0x860640`).
   - `push_back((char)result)` on `out` (`0x85d420`).
   - Skip the 3 bytes (`% H H`), continue.
5. Otherwise (not two hex digits): treat `%` as a literal — find
   the next `%` after position+1 and append the run verbatim, but
   do not decode.

**External calls**:

- `memchr@plt` (`0xcac00`) — twice per iteration
- `__ctype_b_loc@plt` (`0xcb1c0`) — for the `isxdigit` test
- `std::stoi(string const&, size_t*, int)` (`0x860640`)
- `std::string::push_back(char)` (`0x85d420`)
- `std::string::append<__wrap_iter<const char*>>(it, it)` ABI tag
  `ne200100` (`0x111f50`) — verbatim runs
- `operator delete(void*, ulong)` (`0x8c4e80`) — only on unwind /
  long-form deallocation of the 2-char temporary (which is always
  short-form, so this path is dead in practice).

No `.rodata` literals other than the implicit ASCII chars.  No
boost calls.  No static tables.

**Pseudo-C body sketch**

```cpp
std::string zhinst::queryToLink(std::string const& s) {
    std::string out;
    const char* p   = s.data();
    const char* end = p + s.size();
    const char* q   = (const char*)memchr(p, '%', end - p);
    if (!q) q = end;
    out.append(p, q);
    while (q != end) {
        if (end - q >= 3) {
            unsigned char h = (unsigned char)q[1];
            unsigned char l = (unsigned char)q[2];
            const unsigned short* ct = *__ctype_b_loc();
            if ((ct[h] & _ISxdigit) && (ct[l] & _ISxdigit)) {
                std::string tmp(q + 1, 2);                    // short-SSO
                out.push_back((char)std::stoi(tmp, nullptr, 16));
                q += 3;
                // continue: scan for next '%' from q
            } else {
                // not a valid escape, fall through to verbatim
            }
        }
        const char* next = (const char*)memchr(q + 1, '%', end - (q + 1));
        if (!next) next = end;
        out.append(q, next);
        q = next;
    }
    return out;
}
```

The exact branch order in the binary differs slightly (the verbatim
copy is unified for both the "not-enough-bytes" and the "non-hex"
cases) but the observable behaviour is as above.

---

### escapeStringForCsharp

- **Address**: `0x2f9df0`, size 2213B
- **Mangled**: `_ZN6zhinst21escapeStringForCsharpENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string zhinst::escapeStringForCsharp(std::string s)`
  (input passed by value, returned by sret)

**Algorithm**

Same single-helper pattern as `linkToQuery` (eleven calls to
`boost::algorithm::detail::find_format_all_impl2` at `0x2fdd30`) for
ten characters that have C#-style backslash escapes, plus a
**special inline handling for embedded NUL bytes** between escape
passes 3 (`"`) and 4 (BEL).

Pass order (verified by `cmpb` immediates and `.rodata` targets):

| pass | needle           | replacement | needle .rodata | repl .rodata |
|------|------------------|-------------|----------------|--------------|
|  1 | `0x5C` `\`         | `"\\\\"`  | 0x900e42 | 0x90cfd4 |
|  2 | `0x27` `'`         | `"\\'"`   | 0x90a7fa | 0x90d021 |
|  3 | `0x22` `"`         | `"\\\""`  | 0x8fe951 | 0x90cfd7 |
|  -   | (NUL handling, see below)               | | |
|  4 | `0x07` BEL         | `"\\a"`   | 0x90d017 | 0x90d019 |
|  5 | `0x08` BS          | `"\\b"`   | 0x90cfdd | 0x90cfdf |
|  6 | `0x0C` FF          | `"\\f"`   | 0x90cfe2 | 0x90cfe4 |
|  7 | `0x0A` LF          | `"\\n"`   | 0x8ff19b | 0x90cfda |
|  8 | `0x0D` CR          | `"\\r"`   | 0x8ffd43 | 0x90cfe7 |
|  9 | `0x09` TAB         | `"\\t"`   | 0x906c59 | 0x90cfea |
| 10 | `0x0B` VT          | `"\\v"`   | 0x90d01c | 0x90d01e |

Backslash is intentionally first so that the `\X` sequences inserted
later are not re-escaped.  Single-quote is second; the C# `Verbatim
@""` rules are not implied — these are normal C# escapes.

**NUL byte handling** (between passes 3 and 4):

Because `find_format_all_impl2` is given `(begin, end)` ranges and
treats the buffer as opaque bytes, NUL bytes *do* survive the
preceding passes intact.  The compiler emits an SSE2-vectorized
loop (`pcmpeqb %xmm0, ...; paddq` accumulator) at `0x2fa036` that
**counts** the number of NUL bytes in the current string.  If the
count is non-zero, it then:

1. Reserves space in a stack temporary string (calling
   `__shrink_or_extend(unsigned long)` at `0x1142c0`).
2. Walks the original byte-by-byte; for each byte:
   - If non-NUL: append the byte itself (`std::string::append(const
     char*, size_t)` with len 1).
   - If NUL: append the literal `"\0"` (2 bytes from `.rodata` at
     `0x90d024`, which decodes to `\` followed by ASCII `0`).
3. Swaps the rebuilt buffer back into the working string.

The loop pointer trick: `lea 0x612eab(%rip),%rax    # 90d024` loads
the address of the literal `"\0"`, then `cmove %rax,%rsi` selects
between "current byte address" and "literal \0 address" depending on
the byte being NUL or not, then calls `append(ptr, 1+is_nul)` — so
non-NUL appends one byte from the source, NUL appends two bytes from
the literal.

**External calls**:

- `boost::algorithm::detail::find_format_all_impl2<...>` (`0x2fdd30`)
  — 11 times (one of them is for VT, after the NUL handling block)
- `std::string::append(const char*, size_t)` (`0x85ce40`) — for the
  per-byte NUL-rebuild loop
- `std::string::__shrink_or_extend[abi:ne200100](unsigned long)`
  (`0x1142c0`) — pre-reserves the rebuild buffer
- `std::string::__throw_length_error[abi:ne200100]()` (`0xe3ae0`) —
  on size overflow during NUL rebuild
- `operator delete(void*, ulong)` (`0x8c4e80`) — unwind / temporary
  cleanup

**Pseudo-C body sketch**

```cpp
std::string zhinst::escapeStringForCsharp(std::string s) {
    static constexpr struct { char c; const char* repl; } kPre[] = {
        {'\\', "\\\\"}, {'\'', "\\'"}, {'"', "\\\""},
    };
    for (auto [c, r] : kPre)
        boost::algorithm::replace_all(s, std::string(1,c), std::string(r));

    // Count NUL bytes (vectorized).
    size_t nuls = 0;
    for (char ch : s) if (ch == '\0') ++nuls;
    if (nuls != 0) {
        std::string rebuilt;
        rebuilt.reserve(s.size() + nuls);          // size grows by `nuls`
        for (char ch : s) {
            if (ch == '\0') rebuilt.append("\\0", 2);
            else            rebuilt.append(&ch, 1);
        }
        s = std::move(rebuilt);
    }

    static constexpr struct { char c; const char* repl; } kPost[] = {
        {'\a', "\\a"}, {'\b', "\\b"}, {'\f', "\\f"},
        {'\n', "\\n"}, {'\r', "\\r"}, {'\t', "\\t"},
        {'\v', "\\v"},
    };
    for (auto [c, r] : kPost)
        boost::algorithm::replace_all(s, std::string(1,c), std::string(r));

    return s;
}
```

The output is the input wrapped to be safe inside a C# regular
double-quoted string literal (not a verbatim `@""` literal).

---

### replaceUnit

- **Address**: `0x2f7ae0`, size 1860B
- **Mangled**: `_ZN6zhinst11replaceUnitERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_`
- **Signature**: `std::string zhinst::replaceUnit(std::string const& text,
                                                  std::string const& unit,
                                                  std::string const& replacement)`

**Algorithm** (verified end-to-end via GDB on `_seqc_compiler.so`,
IF-266; previous description here was wrong on the probe regex,
the probe flag, **and** the branch payload — see "History"
below.)

Annotates `text` with a unit, picking one of two output shapes
depending on whether `text` already mentions the unit in
parenthesised form:

- "plain prose" inputs (no embedded `(<unit>)`):
  `"<text with trailing [N] stripped> (<replacement>)"`.
- "bracketed-unit" inputs (containing `(<unit>)`):
  every `(<unit>)` occurrence is rewritten to `(<replacement>)`
  via the per-call regex, then a trailing `[N]` index (if any)
  is stripped from the result.

The body is structured as one **probe** + one of two **rewrites**:

1. **Build the per-call regex** from `unit`:

   ```
   pattern = "(.*?) *\\(\\Q" + unit + "\\E\\)(.*)"
   ```
   - prefix `"(.*?) *\\(\\Q"` is assembled in two MOV-immediate
     instructions:
     - 8 bytes `0x5c2a20293f2a2e28` LE = `"(.*?) *\\"`
     - 4 bytes `0x515c285c` LE = `"\\(\\Q"`
   - suffix `"\\E\\)(.*)"` (8 bytes) loaded from `.rodata` at
     `0x90cf68`.
   - The pattern is constructed in a stack-local `std::string`,
     then handed to
     `boost::basic_regex<char,...>::do_assign(begin, end, 0)`
     at `0x176cd0` (called from `0x2f7c4b`); the assembled
     `basic_regex` lives at `-0xc0(%rbp)`.

2. **Build the per-call replacement string** from `replacement`:

   ```
   repl = "$1 (" + replacement + ")$2"
   ```
   - `"$1 ("` from MOV-immediate `0x28203124` (4 bytes).
   - `")$2"` (3 bytes) loaded from `.rodata` at `0x90cf89`.

3. **Probe** at `0x2f7d00` (`+0x220`):

   ```
   regex_match(text.begin(), text.end(), m,
               re,                             // the per-call regex
               boost::regex_constants::match_any /*0x400*/);
   ```

   The `regex` argument is the **per-call `re`** built in step 1
   (loaded from `-0xc0(%rbp)`, the slot just initialised by
   `do_assign`), **not** the static `matchSuffix`.  The flag is
   `match_any` (the engine returns true as soon as any valid
   match is found and does not bother filling out captures
   beyond what's needed for that decision), **not**
   `match_partial`.  The result is saved to `%bl` at `0x2f7d05`
   for use as the branch selector.

   GDB observation, base+`0x2f7d05`:

   | text input        | `%bl` | meaning                       |
   |-------------------|------:|-------------------------------|
   | `"5 V"`           |     0 | no `(V)` substring            |
   | `"a (V) b"`       |     1 | contains `(V)`                |
   | `"abc"`           |     0 | no `(x)` substring            |

4. **Two rewrite paths**, selected by `%bl`:

   - **`%bl == 0`** — plain-prose path (`+0x294 = 0x2f7d74`).
     Strip a trailing `[N]` from `text` via the streaming
     `back_insert_iterator` overload of `regex_replace` at
     `0x307970`, using `matchSuffix` and replacement `"$1"`
     (no-op when no `[N]` is present); then append the literal
     `" ("`, the `replacement` string, and the literal `")"`.
     Output: `"<stripped> (<replacement>)"`.

     This is the path on which the prior recap claimed the
     binary "appends the entire original input" — that was
     factually wrong; the appended payload is `replacement`.

   - **`%bl == 1`** — bracketed-unit path.  Run the
     value-returning `regex_replace(text, re, repl,
     match_default)` overload at `0x2f8230` to substitute every
     `(<unit>)` (matched by the per-call `re`'s anchored form
     `(.*?) *\(\Q<unit>\E\)(.*)`) with `"$1 (<replacement>)$2"`,
     producing a temporary; then run the streaming overload
     again with `matchSuffix` + `"$1"` to strip a trailing
     `[N]` from that temporary into the final `sret` slot.
     Output: `"<text-with-(unit)→(replacement)>"` minus a
     trailing `[N]`.

5. Stack-local `boost::match_results` is destroyed; the per-call
   regex's `shared_ptr<basic_regex_implementation>` is released.

**Static data**:
- `b852d8`: `boost::regex matchSuffix` — function-local static,
  pattern `"(.*?)(?: *\\[[0-9]+\\])+$"` (rodata `0x90cf71`, 25
  chars), built on first call (guarded by `__cxa_guard_*` byte
  at `b852e8`) and registered with `__cxa_atexit` for
  destruction.  Referenced only at the two `lea` sites
  `0x2f7d9d` (plain-prose strip) and `0x2f7f7e` (bracketed-unit
  strip).  **Not used** in the probe.

**External calls** (in order of first use):
- `operator new(unsigned long)` (`0x8c6b30`) — for the assembled
  pattern string when it overflows SSO (≤22 bytes is SSO; the
  pattern is `12 + unit.size() + 8 = 20 + unit.size()`, so any
  `unit` ≥ 3 chars triggers the heap path).
- `memmove@plt` (`0xca890`) — copies `unit` into the assembled
  pattern.
- `std::string::append(const char*, size_t)` (`0x85ce40`)
- `boost::basic_regex<...>::do_assign(const char*, const char*,
  unsigned int)` (`0x176cd0`)
- `boost::regex_match<__wrap_iter<const char*>, ...>` (`0x18b5f0`)
- `std::__shared_weak_count::__release_weak()` (`0x85b9c0`) —
  release the per-call match_results allocator.
- `boost::re_detail_500::string_out_iterator ...
   boost::regex_replace<...>(...)` (`0x307970`) — streaming form,
  used by both branches to strip the `[N]` suffix.
- `boost::regex_replace<..., std::string>(input, regex, fmt,
  flags)` (`0x2f8230`) — value-returning form, used only by the
  bracketed-unit branch for the `(<unit>) → (<replacement>)`
  substitution.
- `std::string::__init_copy_ctor_external` (`0xf14c0`)
- `boost::basic_regex<...>::basic_regex(const char*, unsigned int)`
  (`0x15d560`) — used only for `matchSuffix` construction.
- `boost::basic_regex<...>::~basic_regex()` (`0x15d5c0`) —
  registered via `__cxa_atexit`.
- `std::string::__throw_length_error[abi:ne200100]()` (`0xe3ae0`).

**`.rodata` literals**:
- `0x90cf68`  `"\\E\\)(.*)"`           (8 chars, no NUL terminator)
- `0x90cf71`  `"(.*?)(?: *\\[[0-9]+\\])+$"` (25 chars) — matchSuffix
- `0x90cf89`  `")$2"`                  (3 chars)
- `0x90cf8d`  `"$1"`                   (2 chars + NUL)
- `0x8ff092`  `" ("`                   (2 chars)
- `0x8ff095`  `")"`                    (1 char)

(MOV-immediate literals: `"(.*?) *\\"` (8B), `"\\(\\Q"` (4B),
`"$1 ("` (4B) — these are inlined into the instruction stream and
do not occupy `.rodata`.)

**Pseudo-C body sketch** (matches the verified recon in
`reconstructed/src/core/diagnostics_text.cpp::replaceUnit`):

```cpp
std::string zhinst::replaceUnit(
        std::string const& text,
        std::string const& unit,
        std::string const& replacement) {

    static const boost::regex matchSuffix(
        R"((.*?)(?: *\[[0-9]+\])+$)");          // function-local static

    std::string pattern  = "(.*?) *\\(\\Q" + unit + "\\E\\)(.*)";
    boost::regex re(pattern);

    boost::cmatch m;
    const bool hasParenForm = boost::regex_match(
        text.data(), text.data() + text.size(), m,
        re, boost::regex_constants::match_any);   // per-call re, match_any

    std::string repl = "$1 (" + replacement + ")$2";

    if (!hasParenForm) {
        // plain-prose path: strip trailing [N] (no-op if none),
        // then append " (<replacement>)".
        std::string out;
        boost::regex_replace(
            std::back_inserter(out),
            text.data(), text.data() + text.size(),
            matchSuffix, std::string("$1"));
        out.append(" (", 2);
        out.append(replacement);
        out.append(")", 1);
        return out;
    }
    // bracketed-unit path: substitute (<unit>) → (<replacement>),
    // then strip trailing [N] from the result.
    std::string tmp = boost::regex_replace(text, re, repl);
    std::string out;
    boost::regex_replace(
        std::back_inserter(out),
        tmp.data(), tmp.data() + tmp.size(),
        matchSuffix, std::string("$1"));
    return out;
}
```

**Differential coverage**: 13 inputs in `tests/diff_unreachable/
harness.py::REPLACEUNIT_INPUTS` exercise both branches, including
the `\Q…\E` quoting around metacharacters in `unit` and the `[N]`
stripping in both branches; all 13 match the binary byte-for-byte.

**History**

The original notes here described a "two `regex_replace` passes"
algorithm whose probe used `matchSuffix` with `match_partial` and
whose suffix-stripping branch appended the entire original
`input`.  Each of those three claims (probe regex, probe flag,
append payload) was wrong.  The recon written from those notes
diverged on every harness input.  Resolved during Phase E2c
(IF-266) by GDB-tracing the probe call and the `out.append` site
on the binary; see `incidental_findings.md` IF-266 for the full
trace data and the lessons captured.

---

### browseTo

- **Address**: `0x2eb950`, size 1739B
- **Mangled**: `_ZN6zhinst8browseToENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `bool zhinst::browseTo(std::string path)`
  (returns `bool` in `eax` via `setns`)

**Algorithm**

Opens a file or directory in the user's default browser/file manager
by shelling out to `xdg-open` (Linux only — there is **no** Windows
or macOS branch in this binary).  Performs a small amount of
filesystem walking before invoking the shell:

1. **Walk up the path** while parents exist and are not files:
   - `find_parent_path_size(p)` (boost.filesystem `0x372b50`)
     returns the number of bytes of the parent prefix.
   - If it's zero (no parent), bail out and return `false`.
   - Take the parent prefix and run `boost::filesystem::detail::
     status(parent, &ec)` (`0x36f280`).
   - Loop while `status <= file_not_found` (i.e. `status_error=0` or
     `file_not_found=1`): trim the path to the parent and repeat.

2. **Stat the surviving path** with `boost::filesystem::detail::status`.

3. **If it's a `regular_file`** (status enum value `3`): trim the
   path one more level (its parent directory) — i.e. open the
   *containing folder*, not the file itself.

   Otherwise (directory or other), use the path as-is.

4. **Build the shell command** on the stack:
   - SSO short-form `std::string` initialised with byte `0x14`
     (`= 10 << 1` → length 10) followed by the literal
     `"xdg-open \""` (10 bytes: 9 from the immediate
     `0x6e65706f2d676478` LE = `"xdg-open"`, then 2 bytes from
     `mov $0x2220,%word` = `" \""`, of which the last 2 chars are
     the trailing space + opening double-quote).

     Wait — `0x6e65706f2d676478` LE decodes byte-by-byte as
     `78 64 67 2d 6f 70 65 6e` = `"xdg-open"` (8 bytes), then
     `0x2220` LE = bytes `20 22` = `" \""` (space + double-quote,
     2 bytes), totaling 10 bytes.  Combined with the SSO size byte
     `0x14`, the working string starts as `"xdg-open \""`.

   - Append the chosen path via `std::string::append(p.data(),
     p.size())`.
   - Append the literal `"\""` (1 byte from `.rodata` `0x8fe951`).

5. Call `system@plt` (`0xcb260`) with the constructed command.

6. Return `bool(system_result >= 0)` (`setns %bl`); the value is
   stored back into `eax` and returned.

**Platform branching**: **none observed**.  The binary contains
no path leading to `start`, `open`, `cmd /c`, or any other
non-Linux launcher.  Either this binary was compiled Linux-only or
the Windows/macOS branches are in a different translation unit
that shadows this symbol per platform at link time.

**External calls**:
- `boost::filesystem::detail::path_algorithms::find_parent_path_size`
  (`0x372b50`) — 3 calls (initial, mid-walk, post-stat).
- `boost::filesystem::detail::status(path const&,
  boost::system::error_code*)` (`0x36f280`) — 3 calls.
- `std::string::__init_copy_ctor_external` (`0xf14c0`) — many.
- `std::string::__assign_no_alias<true>` /
  `std::string::__assign_no_alias<false>` (`0xf1270` / `0xf1310`).
- `memmove@plt` (`0xca890`).
- `operator new(unsigned long)` (`0x8c6b30`).
- `operator delete(void*, ulong)` (`0x8c4e80`).
- `std::string::append(const char*, size_t)` (`0x85ce40`).
- `system@plt` (`0xcb260`).
- `std::string::__throw_length_error[abi:ne200100]()` (`0xe3ae0`).

**`.rodata` literals**:
- `0x8fe951` `"\""` (1 char) — closing quote (also reused by
  linkToQuery as the `"` needle).
- The `"xdg-open \""` prefix is materialised from MOV-immediates,
  not `.rodata`.

**Pseudo-C body sketch**

```cpp
bool zhinst::browseTo(std::string p) {
    if (p.empty()) return false;

    // Walk up to find a parent that exists.
    namespace bf = boost::filesystem;
    while (true) {
        size_t pn = bf::path_algorithms::find_parent_path_size(bf::path(p));
        if (pn == 0) return false;
        bf::path parent(p.data(), p.data() + pn);
        bf::file_status st = bf::detail::status(parent);
        if (st.type() > bf::file_not_found) break;       // exists
        p.assign(parent.string());
    }

    // If `p` itself is a regular file, browse to its containing dir.
    bf::file_status st = bf::detail::status(bf::path(p));
    if (st.type() == bf::regular_file) {
        size_t pn = bf::path_algorithms::find_parent_path_size(bf::path(p));
        p.assign(p.data(), pn);
    }

    std::string cmd = "xdg-open \"";
    cmd += p;
    cmd += "\"";
    int rc = std::system(cmd.c_str());
    return rc >= 0;
}
```

**\binarynote**: the shell command is constructed without quoting
or escaping `p`.  A path containing a `"` will break out of the
quoted argument and could allow arbitrary shell injection.  This
is binary-faithful behaviour; document but do not "fix".

---

## Cluster-wide observations

- **Single boost helper for all char-substitution passes**: both
  `linkToQuery` and `escapeStringForCsharp` call the *same*
  `find_format_all_impl2` instantiation at `0x2fdd30` once per
  needle character.  This helper is a candidate to live as a tiny
  static inline in the recon header, or just as a free function in
  `diagnostics_text.cpp` that wraps `boost::algorithm::replace_all`.

- **Function-local statics**: only `replaceUnit` has one
  (`matchSuffix`), with full `__cxa_guard_acquire` / `__cxa_atexit`
  scaffolding.  No other function in the cluster uses statics.

- **`.rodata` neighbours used by the entity-translation siblings**:
  `"&amp;"`, `"&Omega;"`, `"&#937;"`, ... at `0x90ce98..0x90cf3b`
  are not referenced from any of the five functions in this section,
  but ARE the entity table consumed by `entityNameToNumber`,
  `entityNumberToTxt`, `xmlUnescape`, and `xmlUnescapeCopy` — see
  the **D16 batch** section below for the full mapping.  Additional
  consumers (`zhinst::quote`, `zhinst::xmlEscapeCritical`,
  `zhinst::xmlEscapeUtf8Critical`) live at 0x2fa6a0..0x2faaa0 and
  are not yet documented.

- **`browseTo` is platform-specific** to Linux (`xdg-open` only).
  The recon should mark this as a `\binarynote` and either keep
  Linux-only behaviour or stub out non-Linux builds with the same
  bool-returning signature.
---

## D16 batch (xmlUnescape, xmlUnescapeCopy, entityNumberToTxt, entityNameToNumber)

These four are the entity-translation surface of the diagnostics_text
cluster.  Two are entity table walkers (`entityNameToNumber`,
`entityNumberToTxt`), one is a regex-driven XML escape decoder
(`xmlUnescape`), and one is its by-value wrapper (`xmlUnescapeCopy`).

All four operate on libc++ `std::string` (24-byte SSO layout).  The
two entity functions hand-inline `boost::algorithm::replace_all` for
each (search, replacement) pair, with a manual byte-scan early-out
before each call.  `xmlUnescape` uses two function-local
`boost::regex` statics protected by Itanium guard variables.

### xmlUnescape

- **Address**: 0x2fadd0, **size**: 5290 B, **mangled**:
  `_ZN6zhinst11xmlUnescapeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `void xmlUnescape(std::string& s)` (in-place).

#### Function-local statics

| Symbol VMA | Guard VMA | Type | Notes |
|---|---|---|---|
| `0xb85308` | `0xb85318` | `boost::regex` | `xmlUnescape::regex` |
| `0xb85350` | `0xb85360` | `boost::regex` | `escapeMaliciousXmlEscapedSequences::regex` (held in `(anonymous namespace)`) |

Both regex statics are constructed at first use via
`__cxa_guard_acquire` / `boost::basic_regex::basic_regex(char const*, unsigned int)`
with `flags = 0` (default `regex_constants::normal`), and registered
for destruction via `__cxa_atexit`.  **Both are constructed from the
SAME pattern string** at `0x90d057`.

#### Boost regex pattern

`.rodata @ 0x90d057` (NUL-terminated, 26 bytes incl. NUL):

```
&#x[0-9a-fA-F]+;|&#[0-9]+;
```

**Note (corrected 2026-05-12, IF-263):** an earlier draft of this
note claimed a 47-byte pattern that also alternated on the named
entities `&amp;|&lt;|&gt|&quot;|&apos;`.  Direct file-offset readout
of `_seqc_compiler.so` and a GDB trace of the original on `"&amp;"`
both confirm the actual pattern is the 26-byte numeric-only
alternation above.  Named entities pass through `xmlUnescape`
verbatim.  The `xmlEscapeSeqToInt` helper happily decodes the
malformed-but-common `&gt` (no trailing `;`) form for any input that
makes it that far — but our regex won't accept that shape either, so
in practice only the two well-formed numeric forms reach the
helper.  The regex is the **whitelist of valid numeric entity
escapes**; the malicious-sequence escaper uses the same regex
pattern (its own finder logic differs from xmlUnescape's, but the
alternation is shared).

#### External calls observed

| Callee | Purpose |
|---|---|
| `boost::regex_search<__wrap_iter<char const*>, ...>` (0x306a80) | drives the iterator-based scan |
| `zhinst::(anonymous namespace)::xmlEscapeSeqToInt(__wrap_iter<char const*>, __wrap_iter<char const*>)` (0x2fc280) | parses one match into a code-point `unsigned int` |
| `boost::locale::utf::utf_traits<char,1>::encode<back_insert_iterator<string>>(unsigned int, ...)` (0x3149a0) | UTF-8 encode of code point into a temp string |
| `std::string::append<__wrap_iter<char const*>>(...)` (0x111f50) | append iterator range (literal text between matches) |
| `std::string::append(char const*, unsigned long)` (0x85ce40) | append the encoded UTF-8 bytes |
| `std::string::push_back(char)` (0x85d420) | for single-byte ASCII fast path |
| `std::string::__shrink_or_extend[abi:ne200100](unsigned long)` (0x1142c0) | grow temp |
| `std::string::__assign_no_alias<true|false>(char const*, unsigned long)` (0xf1310 / 0xf1270) | swap finalised result back into `s` |
| `std::wstring::__grow_by(...)` (0x85f490) | reused libc++ helper for capacity growth |
| `operator new(unsigned long)` (0x8c6b30), `operator delete(void*, unsigned long)` (0x8c4e80), `memmove@plt` | buffer plumbing |
| `std::__shared_weak_count::__release_weak()` (0x85b9c0) | regex-result cleanup |
| `std::logic_error::logic_error(char const*)` (0x85bbf0) | error path constructor (boost regex `_match_results` "uninitialised" guard etc.) |
| `boost::throw_exception<std::logic_error>(...)` (0x18d110) | actually rethrows |
| `std::string::__throw_length_error[abi:ne200100]()` (0xe3ae0) | length-overflow guard |

The `logic_error` paths all reference one rodata literal at
`0x901eed`: `"Attempt to access an uninitialized boost::match_results<> class."`
This is boost's own error string.  Multiple call sites exist because
each match-results dereference (`(*m)[0]`, `(*m)[1]`, ...) emits its
own guarded check, and each one has its own throw site.

#### Other rodata references

- `0x90ce9b` → `"&amp;"` — used for the literal-`&` lookup in the
  surrogate-pair retry (after decoding a high surrogate, the next
  match is sought with `&` as a sentinel for malformed input).

#### Body sketch

```cpp
void xmlUnescape(std::string& s)
{
    static const boost::regex re(
        "&#x[0-9a-fA-F]+;|&#[0-9]+;");

    boost::match_results<const char*> m;
    std::string out;
    out.reserve(0);  // begins empty

    const char* p   = s.data();
    const char* end = p + s.size();
    bool first = true;            // matches the -0xc4(%rbp) flag

    while (boost::regex_search(p, end, m, re,
                               boost::regex_constants::match_default,
                               /*base=*/p)) {
        const auto& full = m[0];
        out.append(p, full.first);          // pre-match literal
        unsigned cp = (anon)::xmlEscapeSeqToInt(full.first, full.second);

        // Surrogate-pair handling: high surrogate D800..DBFF
        if ((cp & 0xFFFFF800u) == 0xD800u) {
            // need to find the immediately-following low surrogate in
            // the SAME format; do a fresh regex_search anchored at
            // full.second.
            const char* q  = full.second;
            boost::match_results<const char*> m2;
            if (!boost::regex_search(q, end, m2, re, /*flags=*/0, q)
                || m2[0].first != q) {
                throw std::logic_error("...");  // malformed surrogate
            }
            unsigned low = (anon)::xmlEscapeSeqToInt(m2[0].first, m2[0].second);
            if ((low & 0xFFFFFC00u) != 0xDC00u)
                throw std::logic_error("...");
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
            p = m2[0].second;
        } else {
            p = full.second;
        }

        // UTF-8 encode the resulting code point and append.
        boost::locale::utf::utf_traits<char,1>::encode(
            cp, std::back_inserter(out));
        first = false;
    }
    out.append(p, end);   // tail

    // Apply secondary pass: re-escape any malicious entity-like sequences
    // produced by the decode (so that &lt; in source decodes to <, but a
    // raw < in source survives untouched).  This uses the second static
    // regex (b85350) with the SAME pattern but invoked from the
    // (anonymous namespace) escapeMaliciousXmlEscapedSequences helper.
    (anon)::escapeMaliciousXmlEscapedSequences(out);
                  // calls regex_search on b85350 internally

    s.assign(out);  // __assign_no_alias dispatch
}
```

The body is dominated (~3 KB) by Itanium-EH unwind helpers around the
`logic_error` throw paths and by the SSO short/long string
bifurcation that libc++ unrolls for every `data()`/`size()` access on
`s` and on the temp `out`.  Twelve nearly-identical `logic_error`
throw stubs (each doing `lea ...0x901eed... ; logic_error::logic_error
; boost::throw_exception<logic_error>`) account for the giant address
range at the function tail (0x2fbed6..0x2fc080).  These correspond to
the boost match-results uninitialised guard fired at every
`m[i].first` / `m[i].second` access in the encoded body.

#### Notable control flow

- The `(cp & 0xFFFFF800) == 0xD800` test (op `cmp $0xd800,%edi` at
  0x2faff4) is the high-surrogate detector.
- `cmp $0x110000,%r8d` (around 0x2faff... 0x2fb09a) is the
  Unicode-max guard.
- The `cmpb $0x1, -0xc4(%rbp)` sites are the "first iteration?"
  check; on first iteration the regex base equals `p` so the empty
  match handling differs.
- Two `__cxa_guard_acquire/release` blocks (one per regex static)
  with `__cxa_guard_abort` cleanup edges in the catch region.

---

### xmlUnescapeCopy

- **Address**: 0x2fcba0, **size**: 58 B, **mangled**:
  `_ZN6zhinst15xmlUnescapeCopyENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string xmlUnescapeCopy(std::string s)` (by value).

Trivial pass-by-value wrapper around `xmlUnescape`.  Full body:

```nasm
push %rbp ; mov %rsp,%rbp ; push %r14 ; push %rbx
mov %rsi,%rbx                   ; rbx = &s (the by-value copy on caller stack)
mov %rdi,%r14                   ; r14 = return-value buffer (NRVO slot)
mov %rsi,%rdi
call zhinst::xmlUnescape(string&)   ; mutate s in place
mov  0x10(%rbx),%rax            ; copy the libc++ string trailer (size or capacity word)
mov  %rax,0x10(%r14)
movups (%rbx),%xmm0             ; copy first 16 bytes (data ptr + meta) — move semantics
movups %xmm0,(%r14)
xorps %xmm0,%xmm0
movups %xmm0,(%rbx)             ; zero the source (moved-from)
movq   $0x0,0x10(%rbx)
mov %r14,%rax
pop %rbx ; pop %r14 ; pop %rbp ; ret
```

This is a hand-coded **move out of the by-value parameter**: after
the in-place unescape, the 24-byte string control block is bit-blitted
into the return slot and the source is zeroed (libc++ moved-from
state: data=null, size=0, cap=0).  Recon:

```cpp
std::string xmlUnescapeCopy(std::string s)
{
    xmlUnescape(s);
    return s;          // NRVO + move
}
```

---

### entityNumberToTxt

- **Address**: 0x2f4e90, **size**: 4853 B, **mangled**:
  `_ZN6zhinst17entityNumberToTxtERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string entityNumberToTxt(std::string const& s)`.
- **Algorithm**: hand-inlined sequence of 11 substring-search +
  `boost::algorithm::replace_all` operations, one per entity in a
  static table.  No regex, no exceptions on the happy path.

#### Helper called per replacement

`void boost::algorithm::detail::find_format_all_impl2<std::string, first_finderF<char const*, is_equal>, const_formatF<iterator_range<char const*>>, iterator_range<__wrap_iter<char*>>, iterator_range<char const*>>(std::string&, ...)` (`0x2fdd30`)

Stack frame layout per call:
- `-0x50(%rbp)` / `-0x48(%rbp)` = search-pattern range (begin, end) — set ONCE before each manual scan loop.
- `-0x38(%rbp)` / `-0x30(%rbp)` = replacement range (begin, end) — set immediately before the call.
- `-0x40(%rbp)` = the cached "-0x48 word" reused via the `xmm0` move.

The manual byte-by-byte scan loop (visible as a sequence of `cmpb
$<lit>, <off>(%rax,%r11,1) ; jne ...`) is an early-out check: it
streams through the haystack a character at a time, and only invokes
`find_format_all_impl2` if the full needle matches at some position.
This avoids the full boost-finder overhead when the substring is
absent.  The needle length determines the depth of unrolled `cmpb`
checks.

#### Entity table (search → replacement)

| # | Search (rodata) | Bytes | Replacement (rodata) | Bytes |
|---|---|---|---|---|
| 1  | `0x90cef2..0x90cef9` | `&#8201;` | `0x904dda..0x904ddb` | ` ` (space, U+0020) |
| 2  | `0x90cefa..0x90cf00` | `&#178;`  | `0x90cf01..0x90cf03` | `^2` |
| 3  | `0x90cf04..0x90cf0a` | `&#179;`  | `0x90cf0b..0x90cf0d` | `^3` |
| 4  | `0x90cf0e..0x90cf14` | `&#956;`  | `0x901aac..0x901aad` | `m` |
| 5  | `0x90cee1..0x90cee6` | `&#60;`   | `0x900e5b..0x900e5c` | `<` |
| 6  | `0x90ceec..0x90cef1` | `&#62;`   | `0x900e5d..0x900e5e` | `>` |
| 7  | `0x90cf15..0x90cf1a` | `&#38;`   | `0x90ce47..0x90ce48` | `&` |
| 8  | `0x90cf1b..0x90cf22` | `&#8486;` | `0x90cf23..0x90cf26` | `Ohm` |
| 9  | `0x90cf27..0x90cf2c` | `&#43;`   | `0x900e38..0x900e39` | `+` |
| 10 | `0x90cf2d..0x90cf34` | `&#8722;` | `0x900e46..0x900e47` | `-` |
| 11 | `0x90cf35..0x90cf3b` | `&#215;`  | `0x900e73..0x900e74` | `x` |

Note: the replacements are NOT contiguous in rodata; many are single
characters borrowed from other char-array literals elsewhere in
`.rodata` (printf-format scratch, regex char-class buffers, etc.) so
the linker can dedupe.  Read each replacement byte literally from
the offset.

#### Body sketch

```cpp
std::string entityNumberToTxt(std::string const& src)
{
    std::string s = src;          // SSO-aware copy (init_copy_ctor_external)

    static constexpr struct { const char* find; const char* repl; }
        TABLE[] = {
            { "&#8201;", " " },
            { "&#178;",  "^2"  },
            { "&#179;",  "^3"  },
            { "&#956;",  "m"   },
            { "&#60;",   "<"   },
            { "&#62;",   ">"   },
            { "&#38;",   "&"   },
            { "&#8486;", "Ohm" },
            { "&#43;",   "+"   },
            { "&#8722;", "-"   },
            { "&#215;",  "x"   },
        };

    for (auto const& e : TABLE) {
        // Manual byte scan — inlined first_finder
        if (contains(s, e.find))
            boost::algorithm::replace_all(s, e.find, e.repl);
    }
    return s;
}
```

The manual contains check is essential: the unrolled `cmpb` loop is
~50 B per entity, much cheaper than a full boost-finder invocation
when the entity is absent (the common case for short config strings).

---

### entityNameToNumber

- **Address**: 0x2f4290, **size**: 3061 B, **mangled**:
  `_ZN6zhinst18entityNameToNumberERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Signature**: `std::string entityNameToNumber(std::string const& s)`.
- **Algorithm**: same shape as `entityNumberToTxt` (hand-inlined
  scan + replace_all per entry), but with only **7** entries — the
  inverse direction is lossy (multiple named entities can map to the
  same numeric form, but only one canonical name is used here).

#### Entity table (search → replacement)

| # | Search (rodata) | Bytes | Replacement (rodata) | Bytes |
|---|---|---|---|---|
| 1 | `0x90ce9b..0x90cea0` | `&amp;`    | `0x90ce47..0x90ce48` | `&` |
| 2 | `0x90cea1..0x90cea8` | `&Omega;`  | `0x90cea9..0x90ceaf` | `&#937;` |
| 3 | `0x90ceb0..0x90ceb5` | `&deg;`    | `0x90ceb6..0x90cebc` | `&#176;` |
| 4 | `0x90cebd..0x90cec4` | `&Theta;`  | `0x90cec5..0x90cecb` | `&#952;` |
| 5 | `0x90cecc..0x90ced4` | `&plusmn;` | `0x90ced5..0x90cedb` | `&#177;` |
| 6 | `0x90cedc..0x90cee0` | `&lt;`     | `0x90cee1..0x90cee6` | `&#60;` |
| 7 | `0x90cee7..0x90ceeb` | `&gt;`     | `0x90ceec..0x90cef1` | `&#62;` |

#### External calls

Identical helper set to `entityNumberToTxt`:

- `std::string::__init_copy_ctor_external(char const*, unsigned long)` (0xf14c0) — once for the local copy of `s`.
- `boost::algorithm::detail::find_format_all_impl2<...>` (0x2fdd30) — 7 calls.
- `operator delete(void*, unsigned long)` (0x8c4e80) — exception-cleanup edge.
- `_Unwind_Resume@plt` (0xca850) — terminal landing pad.

#### Body sketch

```cpp
std::string entityNameToNumber(std::string const& src)
{
    std::string s = src;

    static constexpr struct { const char* find; const char* repl; }
        TABLE[] = {
            { "&amp;",    "&"      },   // Note: appears FIRST so that
                                        // subsequent &<name>; entities
                                        // are not double-escaped — but
                                        // since the inputs to this fn
                                        // are entity NAMES not raw &,
                                        // the order is benign.
            { "&Omega;",  "&#937;" },
            { "&deg;",    "&#176;" },
            { "&Theta;",  "&#952;" },
            { "&plusmn;", "&#177;" },
            { "&lt;",     "&#60;"  },
            { "&gt;",     "&#62;"  },
        };

    for (auto const& e : TABLE) {
        if (contains(s, e.find))
            boost::algorithm::replace_all(s, e.find, e.repl);
    }
    return s;
}
```

Tables for the two functions are NOT inverses of each other — the
forward direction recognises a wider numeric range (11 entries
including thinsp, micro, ohm, plus, minus, mult sign, superscripts)
than the backward direction (7 named entities).  This asymmetry
matches an XML output pipeline where text is normalised to numeric
references on output but only a curated set of named entities is
recognised on input.

#### Recon ordering hint

Because both functions share the `find_format_all_impl2` helper and
identical `iterator_range<char const*>` argument-passing convention,
the recon should put both tables in the same TU (`diagnostics_text.cpp`)
and share a small inline helper:

```cpp
template <std::size_t N>
static std::string apply_entity_table(
    std::string s,
    std::array<std::pair<std::string_view, std::string_view>, N> const& tbl)
{
    for (auto const& [find, repl] : tbl)
        if (s.find(find) != std::string::npos)
            boost::algorithm::replace_all(s, find, repl);
    return s;
}
```

This will not produce byte-identical assembly (the binary's
unrolled-cmpb scan is too aggressive), but it preserves all
observable behaviour and the entity tables are the only ground-truth
data anyway.  `\verifyme` the two functions against the difftest
suite once reconstructed.


---

### quote (string&) — 20th symbol, not in D14 inventory

- **Mangled**: `_ZN6zhinst5quoteERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: 0x2fa6a0 | **Size**: 311 B
- **Callers**: none in-cluster (leaf)
- **String evidence**:
  - Finder `[0x8fe951, 0x8fe952)` = byte `"` (single double-quote)
  - Formatter `[0x90cfd7, 0x90cfd9)` = bytes `\` `"` (2-char escape `\"`)
- **Algorithm**:
  ```
  void quote(string& s) {
    // Step 1: if s contains '"', replace each with \"
    if (memchr(s.data(), '"', s.size())) {
      boost::algorithm::find_format_all_impl2(
        s,
        first_finder("\"" /*1 char*/),
        const_formatter("\\\"" /*2 chars*/));
    }
    // Step 2: wrap in double quotes
    s.insert(s.begin(), '"');
    s.push_back('"');
  }
  ```
- Inline `memchr` is open-coded as a byte-loop before the `find_format_all_impl2` call (avoids constructing the boost finder when no `"` is present).
- The `string::__shrink_or_extend` call ensures capacity for the +2-byte growth before the `insert`/`push_back` (saves a realloc).

