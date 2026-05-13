"""
tests/diff_unreachable/harness.py

Diff-test harness for binding-unreachable reconstructions
(Phase E of the seqc_compiler reconstruction).

Loads two shared libraries via ctypes:

  - reference:  ./_seqc_compiler.so (the original binary; no
                exported helper symbols, called by raw text-segment
                offset)
  - candidate:  reconstructed/build-libcxx-test/libdiagtxt_libcxx.so
                (the libc++ build of the reconstructed surface;
                exports symbols by mangled libc++ name)

For each (symbol, input) pair, both implementations are invoked
through a libc++ std::string passed from a small C-ABI shim
(`shim.cpp`, also linked into libdiagtxt_libcxx.so).  Outputs
(returned strings or in-place-mutated arguments) are compared
byte-for-byte.

This is **not** part of the regular `python tests/diff_test_fast.py`
suite; it requires the libc++ test .so to exist (see
`tests/diff_unreachable/build.sh`) and runs separately.

Usage:
    python tests/diff_unreachable/harness.py [--filter SYMBOL]
"""
from __future__ import annotations

import argparse
import ctypes
import os
import sys
from dataclasses import dataclass
from typing import Callable, Iterable

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ORIGINAL_SO = os.path.join(REPO_ROOT, "_seqc_compiler.so")
CANDIDATE_SO = os.path.join(
    REPO_ROOT, "reconstructed", "build-libcxx-test", "libdiagtxt_libcxx.so"
)

# ---------------------------------------------------------------- #
# Color output (mirrors diff_test_fast.py conventions).
# ---------------------------------------------------------------- #
USE_COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR", "") == ""

def _c(code: str, s: str) -> str:
    return f"\033[{code}m{s}\033[0m" if USE_COLOR else s

GREEN = lambda s: _c("32", s)
RED   = lambda s: _c("31", s)
DIM   = lambda s: _c("2",  s)
BOLD  = lambda s: _c("1",  s)


# ---------------------------------------------------------------- #
# Library loading and address resolution.
# ---------------------------------------------------------------- #

def find_text_base(path: str) -> int:
    """Return the runtime base address of the executable (r-xp) PT_LOAD
    of the .so identified by `path`, by parsing /proc/self/maps."""
    realpath = os.path.realpath(path)
    with open("/proc/self/maps") as f:
        for line in f:
            if realpath not in line and path not in line:
                continue
            # Match the executable mapping (r-xp permissions).
            parts = line.split()
            if len(parts) < 6:
                continue
            perms = parts[1]
            if "x" not in perms:
                continue
            base_hex = parts[0].split("-")[0]
            return int(base_hex, 16)
    raise RuntimeError(f"could not locate executable mapping for {path}")


# Load both libraries.  RTLD_LOCAL keeps the original's statically-
# linked libc++ symbols from shadowing the candidate's dynamic ones.
#
# RTLD_LAZY is essential for the candidate: libdiagtxt_libcxx.so is
# linked with -Wl,--unresolved-symbols=ignore-all so we don't have to
# drag the entire device subtree into the test build.  Those undefined
# symbols (e.g. zhinst::detail::Uhfawg::Uhfawg) live in code paths the
# first-pass mutators never reach, but a strict (RTLD_NOW) dlopen
# would still refuse to load the .so.  ctypes.RTLD_LAZY is None on
# some Pythons; os.RTLD_LAZY (= 1) is the portable spelling.
_LAZY_LOCAL = os.RTLD_LAZY | os.RTLD_LOCAL
_orig = ctypes.CDLL(ORIGINAL_SO, mode=_LAZY_LOCAL)
if not os.path.isfile(CANDIDATE_SO):
    print(RED(f"FATAL: {CANDIDATE_SO} not found.\n"
              "       Run tests/diff_unreachable/build.sh first."),
          file=sys.stderr)
    sys.exit(2)
_cand = ctypes.CDLL(CANDIDATE_SO, mode=_LAZY_LOCAL)

ORIG_BASE = find_text_base(ORIGINAL_SO)


# ---------------------------------------------------------------- #
# libc++ std::string helpers (via shim symbols in candidate .so).
# ---------------------------------------------------------------- #

_make = _cand.diff_unreachable_string_make
_make.restype = ctypes.c_void_p
_make.argtypes = [ctypes.c_char_p, ctypes.c_size_t]

_make_empty = _cand.diff_unreachable_string_make_empty
_make_empty.restype = ctypes.c_void_p
_make_empty.argtypes = [ctypes.c_size_t]

_free = _cand.diff_unreachable_string_free
_free.restype = None
_free.argtypes = [ctypes.c_void_p]

_data = _cand.diff_unreachable_string_data
_data.restype = ctypes.POINTER(ctypes.c_char)
_data.argtypes = [ctypes.c_void_p]

_size = _cand.diff_unreachable_string_size
_size.restype = ctypes.c_size_t
_size.argtypes = [ctypes.c_void_p]

# sret helpers — see shim.cpp.
_alloc_uninit = _cand.diff_unreachable_string_alloc_uninit
_alloc_uninit.restype = ctypes.c_void_p
_alloc_uninit.argtypes = []

_free_uninit = _cand.diff_unreachable_string_free_uninit
_free_uninit.restype = None
_free_uninit.argtypes = [ctypes.c_void_p]

_destroy_in_place = _cand.diff_unreachable_string_destroy_in_place
_destroy_in_place.restype = None
_destroy_in_place.argtypes = [ctypes.c_void_p]

_place_construct = _cand.diff_unreachable_string_place_construct
_place_construct.restype = None
_place_construct.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]

# Exception-safe trampoline (see shim.cpp).  Lets the harness invoke
# functions that may throw without aborting the Python process.
_try_pod_u64_cref2 = _cand.diff_unreachable_try_pod_u64_cref2
_try_pod_u64_cref2.restype = ctypes.c_int
_try_pod_u64_cref2.argtypes = [
    ctypes.c_void_p,  # fn pointer
    ctypes.c_void_p,  # const string* a
    ctypes.c_void_p,  # const string* b
    ctypes.POINTER(ctypes.c_uint64),  # out value
    ctypes.c_char_p,  # what buffer
    ctypes.c_size_t,  # what cap
]


def make_string(b: bytes) -> int:
    """Heap-allocate a libc++ std::string from `b`; return its address."""
    return _make(b, len(b))

def free_string(p: int) -> None:
    _free(p)

def string_bytes(p: int) -> bytes:
    n = _size(p)
    if n == 0:
        return b""
    buf = _data(p)
    return ctypes.string_at(buf, n)


def alloc_uninit_slot() -> int:
    """Raw 24-byte slot for sret-returned std::string."""
    return _alloc_uninit()

def destroy_and_free_slot(p: int) -> None:
    _destroy_in_place(p)
    _free_uninit(p)


# ---------------------------------------------------------------- #
# Symbol resolution.
# ---------------------------------------------------------------- #

def orig_fn(offset: int, restype, argtypes) -> Callable:
    """Build a ctypes-callable for an original-side function at the
    given binary text-VMA offset."""
    proto = ctypes.CFUNCTYPE(restype, *argtypes)
    return proto(ORIG_BASE + offset)

def cand_fn(mangled: str, restype, argtypes) -> Callable:
    """Resolve a candidate-side function by mangled symbol name."""
    fn = getattr(_cand, mangled)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


# ---------------------------------------------------------------- #
# Per-symbol calling conventions.
# ---------------------------------------------------------------- #
#
# Each entry below describes one in-scope D16 symbol and how to
# invoke it from both sides.  The caller pattern matches the C++
# signature:
#
#   void f(string&)         -> call(p):       fn(p)             ; result = string_bytes(p)
#   void f(string&, size_t) -> call(p, n):    fn(p, n)          ; result = string_bytes(p)
#   string f(const string&) -> call(p):       q = make_empty(); fn_sret(q, p) ... see notes below
#
# The Itanium ABI passes by-value `std::string` by reference at
# call site (the parameter is constructed from the argument), and
# returns `std::string` via a hidden first pointer (sret).  ctypes
# does not natively know about C++ value semantics, so we stick to
# the in-place mutators for the first harness pass and add the
# return-by-value variants once the basic plumbing is verified.
#
# First-pass coverage: only `void f(string&)` / `void f(string&, size_t)`
# symbols, which is 8 of the 19 in-scope:
#   xmlUnescape, xmlEscapeCritical, xmlEscapeUtf8Critical,
#   escapeStringForJson, sanitizeFilename, sanitizeInvalidFilename,
#   truncateUtf8Safe, truncateXmlSafe, quote.
# (That's 9 — counted as one batch.)
#
# E2a extension: sret return-by-value symbols.  `string f(...)` lowers
# to `void f(string* sret, ...)` under the Itanium ABI — the caller
# owns 24 B of raw storage at `sret` and the callee placement-
# constructs the result there.  For by-value `string` parameters
# (`xmlUnescapeCopy`, `escapeStringFor{Csharp,Python}`) the caller
# constructs a copy into heap-owned storage and passes the pointer;
# the callee owns destruction.


@dataclass(frozen=True)
class Symbol:
    name: str
    orig_offset: int
    cand_mangled: str
    # 'inplace'      = void(string&)
    # 'inplace_n'    = void(string&, size_t)
    # 'sret_cstr'    = string(const char*)            -> void(sret, c_char_p)
    # 'sret_cref'    = string(const string&)          -> void(sret, string*)
    # 'sret_byval'   = string(string)                 -> void(sret, string*) [callee destroys arg]
    # 'sret_cref3'   = string(const string&,
    #                         const string&,
    #                         const string&)          -> void(sret, string*, string*, string*)
    # 'pod_u32_cref' = uint32_t f(const string&)      -> uint32_t(string*) [ret in %eax]
    # 'pod_u64_cref2'= uint64_t f(const string&,
    #                              const string&)      -> uint64_t(string*, string*) [ret in %rax]
    kind: str

# Curated D16 in-place mutators only (first-pass scope).
SYMBOLS: list[Symbol] = [
    Symbol("xmlEscapeCritical",      0x2fa7e0,
           "_ZN6zhinst17xmlEscapeCriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("xmlEscapeUtf8Critical",  0x2faaa0,
           "_ZN6zhinst21xmlEscapeUtf8CriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("xmlUnescape",            0x2fadd0,
           "_ZN6zhinst11xmlUnescapeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("escapeStringForJson",    0x2f89b0,
           "_ZN6zhinst19escapeStringForJsonERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("sanitizeFilename",       0x2fcbe0,
           "_ZN6zhinst16sanitizeFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("sanitizeInvalidFilename",0x2fd3f0,
           "_ZN6zhinst23sanitizeInvalidFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("quote",                  0x2fa6a0,
           "_ZN6zhinst5quoteERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "inplace"),
    Symbol("truncateUtf8Safe",       0x2fca40,
           "_ZN6zhinst16truncateUtf8SafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm",
           "inplace_n"),
    Symbol("truncateXmlSafe",        0x2fc690,
           "_ZN6zhinst15truncateXmlSafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm",
           "inplace_n"),
    # ---- E2a sret return-by-value batch ----
    Symbol("toCheckedString",        0x2f2700,
           "_ZN6zhinst15toCheckedStringEPKc",
           "sret_cstr"),
    Symbol("entityNumberToTxt",      0x2f4e90,
           "_ZN6zhinst17entityNumberToTxtERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_cref"),
    Symbol("entityNameToNumber",     0x2f4290,
           "_ZN6zhinst18entityNameToNumberERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_cref"),
    Symbol("linkToQuery",            0x2f2f20,
           "_ZN6zhinst11linkToQueryERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_cref"),
    Symbol("queryToLink",            0x2f4030,
           "_ZN6zhinst11queryToLinkERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_cref"),
    Symbol("xmlUnescapeCopy",        0x2fcba0,
           "_ZN6zhinst15xmlUnescapeCopyENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_byval"),
    Symbol("escapeStringForCsharp",  0x2f9df0,
           "_ZN6zhinst21escapeStringForCsharpENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_byval"),
    Symbol("escapeStringForPython",  0x2f9780,
           "_ZN6zhinst21escapeStringForPythonENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_byval"),
    # ---- E2c: 3-arg sret ----
    Symbol("replaceUnit",            0x2f7ae0,
           "_ZN6zhinst11replaceUnitERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_",
           "sret_cref3"),
    # ---- IF-267: scalar return, single string& argument ----
    Symbol("toDeviceFamily",         0x2debd0,
           "_ZN6zhinst14toDeviceFamilyERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "pod_u32_cref"),
    # ---- E2c: 64-bit POD return, two const string& args ----
    Symbol("generateSfc",            0x2d10b0,
           "_ZN6zhinst11generateSfcERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_",
           "pod_u64_cref2"),
    # ---- F2: infra/calver.cpp — POD-only batch (9 symbols) ----
    # Single-blob accessors: size_t (CalVer const&) — return in %rax.
    Symbol("CalVer::year",           0x100220,
           "_ZNK6zhinst6CalVer4yearEv",
           "pod_u64_blob"),
    Symbol("CalVer::month",          0x100230,
           "_ZNK6zhinst6CalVer5monthEv",
           "pod_u64_blob"),
    Symbol("CalVer::patch",          0x100240,
           "_ZNK6zhinst6CalVer5patchEv",
           "pod_u64_blob"),
    Symbol("CalVer::build",          0x100250,
           "_ZNK6zhinst6CalVer5buildEv",
           "pod_u64_blob"),
    # uint32_t (CalVer const&) — return in %eax.
    Symbol("asDecimal",              0x1006e0,
           "_ZN6zhinst9asDecimalERKNS_6CalVerE",
           "pod_u32_blob"),
    Symbol("asBinary",               0x1007c0,
           "_ZN6zhinst8asBinaryERKNS_6CalVerE",
           "pod_u32_blob"),
    # bool (CalVer const&) and bool (array<size_t,3> const&).
    Symbol("isSet(CalVer)",          0x100470,
           "_ZN6zhinst5isSetERKNS_6CalVerE",
           "pod_bool_blob"),
    Symbol("isSet(array)",           0x1020d0,
           "_ZN6zhinst5isSetERKNSt3__15arrayImLm3EEE",
           "pod_bool_blob"),
    # bool (CalVer const&, CalVer const&) — comparison operators.
    Symbol("operator==(CalVer)",     0x100bc0,
           "_ZN6zhinsteqERKNS_6CalVerES2_",
           "pod_bool_blob2"),
    Symbol("operator<(CalVer)",      0x100c00,
           "_ZN6zhinstltERKNS_6CalVerES2_",
           "pod_bool_blob2"),
    # No-arg sret string return.
    Symbol("getLaboneVersionWithCommitHash", 0x1002a0,
           "_ZN6zhinst30getLaboneVersionWithCommitHashEv",
           "sret_void"),
    # No-arg sret CalVer (32-byte POD blob) return.
    Symbol("getLaboneVersion",       0x100270,
           "_ZN6zhinst16getLaboneVersionEv",
           "sret_blob_void"),
    # std::string sret from single blob arg.
    Symbol("toString(CalVer)",       0x1007f0,
           "_ZN6zhinst8toStringERKNS_6CalVerE",
           "sret_blob"),
    Symbol("toString(array)",        0x101d80,
           "_ZN6zhinst8toStringERKNSt3__15arrayImLm3EEE",
           "sret_blob"),
    # CalVer sret from uint32 arg.
    Symbol("fromDecimal(u32)",       0x100490,
           "_ZN6zhinst11fromDecimalEj",
           "sret_blob_u32"),
    Symbol("fromBinary",             0x100780,
           "_ZN6zhinst10fromBinaryEj",
           "sret_blob_u32"),
    # Identity reinterpret: array<size_t,3> const& (CalVer const&) — returns
    # the same pointer in %rax (treated as opaque blob reference).
    Symbol("CalVer::triple",         0x100260,
           "_ZNK6zhinst6CalVer6tripleEv",
           "ref_blob"),
    # CalVer(string const&) C2 base-ctor — writes 32-byte CalVer blob
    # into the *this slot supplied in %rdi from a parsed version
    # string in %rsi.  Restricted to non-throwing inputs (empty + dotted
    # numeric forms); throwing inputs are deferred to a separate shape.
    Symbol("CalVer::ctor(string)",   0xffdb0,
           "_ZN6zhinst6CalVerC2ERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE",
           "ctor_blob_cref"),
]


# ---------------------------------------------------------------- #
# Curated input corpora.  Per-symbol; defaulting to a generic set.
# ---------------------------------------------------------------- #

GENERIC_INPUTS: list[bytes] = [
    b"",
    b"a",
    b"hello",
    b"hello, world",
    b"a" * 22,                              # libc++ SSO max
    b"a" * 23,                              # forces long-form
    b"a" * 1024,                            # large
    b"\x00\x01\x02",                        # control bytes
    b"\xc3\xa9",                            # UTF-8 é
    b"\xe2\x9c\x93",                        # UTF-8 ✓
    b"\xf0\x9f\x98\x80",                    # UTF-8 😀
    b"\xff\xfe\xfd",                        # high bytes
    b"\xc3",                                # malformed UTF-8 (truncated)
]

PER_SYMBOL_EXTRA: dict[str, list[bytes]] = {
    "xmlEscapeCritical": [
        b"&", b"<", b">", b"&amp;", b"&lt;", b"&gt;",
        b"a&b<c>d", b"&#65;", b"&#x41;", b"&unknown;",
        b"&amp;amp;",
    ],
    "xmlEscapeUtf8Critical": [
        b"\x80", b"\xa0\xff",
        b"normal then \xff\xfe\xfd",
    ],
    "xmlUnescape": [
        b"&amp;", b"&lt;", b"&gt;", b"&quot;",
        b"&#65;", b"&#x41;", b"&#1234;",
        b"&amp;amp;",                       # IF-262 target
        b"a &amp; b", b"&unknown;",
        b"&amp",                            # missing semicolon
    ],
    "escapeStringForJson": [
        b'\\', b'"', b'\n', b'\r', b'\t', b'\b', b'\f',
        b'\x00abc', b'\x1f',
        b'a"b\\c\nd',
    ],
    "sanitizeFilename": [
        b"foo/bar", b"foo\\bar", b"foo:bar", b"foo*bar",
        b"foo?bar", b"foo<bar>baz", b"foo|bar",
        b"../etc/passwd", b"..\\windows",
    ],
    "sanitizeInvalidFilename": [
        b"CON", b"PRN", b"AUX", b"NUL",
        b"COM1", b"COM9", b"LPT1", b"LPT9",
        b"COM1.txt", b"PRN.log",
        b"normal.txt",
    ],
    "quote": [
        b"", b"plain", b'has "quotes" inside',
        b"trailing space ", b" leading space",
    ],
    "truncateUtf8Safe": [
        b"abc", b"abcdef", b"\xc3\xa9\xc3\xa9\xc3\xa9",
        b"a\xe2\x9c\x93b",
    ],
    "truncateXmlSafe": [
        b"abc&amp;def", b"abc&lt;def&gt;", b"a&#65;b",
    ],
    "toCheckedString": [
        # Curated separately (allows None for nullptr); see iter_inputs.
    ],
    "entityNumberToTxt": [
        b"&#65;", b"&#x41;", b"&#1234;", b"&#0;", b"&#1114111;",
        b"&#x10ffff;", b"&#xff;", b"&#9;",
        b"&unknown;", b"&amp;", b"plain text",
    ],
    "entityNameToNumber": [
        b"&amp;", b"&lt;", b"&gt;", b"&quot;", b"&apos;",
        b"&nbsp;", b"&copy;", b"&unknown;", b"&", b"plain",
    ],
    "linkToQuery": [
        b"http://example.com/path?a=1",
        b"a b c", b"hello+world", b"%20encoded%20",
        b"key=value&other=thing",
    ],
    "queryToLink": [
        b"a%20b", b"hello+world", b"%2F%3F",
        b"key=value&other=thing", b"plain",
    ],
    "xmlUnescapeCopy": [
        b"&amp;", b"&lt;", b"&gt;", b"&quot;",
        b"&#65;", b"&#x41;", b"&#1234;",
        b"&amp;amp;", b"a &amp; b", b"&unknown;", b"&amp",
    ],
    "escapeStringForCsharp": [
        b'\\', b'"', b'\n', b'\r', b'\t',
        b'\x00abc', b'\x1f',
        b'a"b\\c\nd',
    ],
    "escapeStringForPython": [
        b'\\', b'"', b"'", b'\n', b'\r', b'\t',
        b'\x00abc', b'\x1f',
        b"a'b\"c\\d\ne",
    ],
    # IF-267 — exercises every fastpath and every map prefix, plus
    # case-sensitivity and edge lengths.  Inputs cover:
    #   - empty (jump-table[0]: returns Unknown=0, distinct from 0x800)
    #   - lengths 1..7 with and without map-key matches
    #   - the 4-byte "none" literal fastpath
    #   - the 6-byte "SHFACC" literal fastpath
    #   - the 7-byte "DEFAULT", "SHFPPC2", "SHFPPC4" literal fastpaths
    #   - lower-case counterparts of every uppercase prefix (must
    #     fall through to the 0x800 sentinel)
    #   - strings longer than 7 (bypasses the jump table) with and
    #     without map-key prefixes
    #   - inputs starting with a key-prefix substring but with a
    #     suffix (boost::starts_with semantics)
    "toDeviceFamily": [
        b"",                                  # jump-table[0] -> 0
        b"a", b"H", b"S",                     # len 1, no fastpath/map -> 0x800
        b"MF", b"mf", b"HF", b"hf", b"UH",    # len 2: only "MF" hits map
        b"SHF", b"shf", b"GHF", b"VHF",       # len 3 map keys
        b"HF2", b"UHF", b"MFI", b"HDA",
        b"none", b"NONE", b"MFLI", b"MFIA",   # len 4: none + map prefixes
        b"PQSC", b"QHUB", b"HDAW", b"SHFA",
        b"SHFLI", b"UHFLI", b"HDAWG",         # len 5
        b"SHFQC", b"shfli", b"PQSC1",
        b"HWMOCK", b"hwmock", b"SHFACC",      # len 6: SHFACC fastpath
        b"shfacc", b"HDAWG8", b"hdawg8",      # case-sensitive
        b"SHFSG2", b"SHFQA4", b"UHFAWG",
        b"GHFLI", b"VHFLI",
        b"DEFAULT", b"default", b"Default",   # len 7: DEFAULT fastpath
        b"SHFPPC2", b"shfppc2", b"SHFPPC4",
        b"SHFPPC8",                           # similar but no fastpath
        b"HDAWG88", b"SHFQA42",               # len 7 falling through to map
        b"HDAWG822", b"Something", b"shfqa4", # len > 7 (bypasses jump table)
        b"VHFLI24", b"UHFLI88",
    ],
}

# replaceUnit takes 3 strings: (text, unit, replacement).  Curated
# triples target each documented behaviour: literal substitution,
# the `\Q...\E`-bracketed exclusion, and unit boundaries.
REPLACEUNIT_INPUTS: list[tuple[bytes, bytes, bytes]] = [
    (b"5 V", b"V", b"volt"),
    (b"5V and 10V", b"V", b"volt"),
    (b"", b"V", b"volt"),
    (b"5 V", b"", b"x"),
    (b"5 V", b"V", b""),
    (b"hello world", b"world", b"earth"),
    (b"abc", b"x", b"y"),                  # no match
    (b"VVV", b"V", b"volt"),               # adjacent matches
    (b"a (V) b", b"V", b"volt"),           # parenthesised — \Q...\E exclusion?
    (b"5\\E V", b"V", b"volt"),            # literal \E in input
    (b"5 V 10 V 15 V", b"V", b"_"),
    (b"\xff\xfe V \xfd", b"V", b"x"),      # high bytes around the unit
    (b"a" * 100 + b" V", b"V", b"volt"),   # long-form input
]

# toCheckedString takes `const char*`.  None encodes nullptr; the
# rest are encoded as bytes and passed via ctypes.c_char_p.
TOCHECKEDSTRING_INPUTS: list[bytes | None] = [
    None,                               # nullptr
    b"",                                # empty C string
    b"a",
    b"hello",
    b"hello, world",
    b"a" * 22,
    b"a" * 23,
    b"\x01\x02",                        # control bytes (no embedded NUL)
    b"\xc3\xa9",                        # UTF-8
]

TRUNCATE_LIMITS = [0, 1, 2, 3, 5, 7, 10, 22, 23, 100]

# ----------------------------------------------------------------- #
# CalVer / array<size_t,3> blob inputs.  CalVer is 4 x size_t = 32 B
# (year_, month_, patch_, build_); array<size_t,3> is 3 x size_t =
# 24 B.  Both are POD and passed via const-ref (i.e. by pointer)
# under the Itanium ABI, so the harness only needs raw bytes.
# ----------------------------------------------------------------- #

def _pack_calver(year: int, month: int, patch: int, build: int) -> bytes:
    """Pack a 32-byte CalVer struct (4 x size_t little-endian)."""
    import struct
    return struct.pack("<QQQQ", year, month, patch, build)

def _pack_arr3(a: int, b: int, c: int) -> bytes:
    """Pack a 24-byte std::array<size_t,3>."""
    import struct
    return struct.pack("<QQQ", a, b, c)

# Curated CalVer values: zero, common LabOne versions, field-boundary
# stressors, and asDecimal/asBinary edge cases.
CALVER_INPUTS: list[tuple[bytes, str]] = [
    (_pack_calver(0, 0, 0, 0),                "zero"),
    (_pack_calver(26, 1, 3, 9),               "26.01.3.9 (LabOne)"),
    (_pack_calver(25, 12, 0, 0),              "25.12.0.0"),
    (_pack_calver(1, 1, 1, 1),                "1.1.1.1"),
    (_pack_calver(99, 99, 9, 9999),           "asDecimal-max-fields"),
    (_pack_calver(100, 100, 10, 10000),       "asDecimal-overflow-by-1"),
    (_pack_calver(255, 255, 15, 4095),        "asBinary-max-fields"),
    (_pack_calver(256, 256, 16, 4096),        "asBinary-overflow-by-1"),
    (_pack_calver(0, 0, 0, 1),                "build-only"),
    (_pack_calver(0, 0, 1, 0),                "patch-only"),
    (_pack_calver(0, 1, 0, 0),                "month-only"),
    (_pack_calver(1, 0, 0, 0),                "year-only"),
    (_pack_calver(0xFFFFFFFFFFFFFFFF, 0, 0, 0), "year=u64-max"),
]

# Pairs of CalVer values for binary operators.
CALVER_PAIRS: list[tuple[bytes, bytes, str]] = [
    (_pack_calver(0, 0, 0, 0),  _pack_calver(0, 0, 0, 0),  "0==0"),
    (_pack_calver(26, 1, 3, 9), _pack_calver(26, 1, 3, 9), "26.01.3.9 ==self"),
    (_pack_calver(26, 1, 3, 9), _pack_calver(26, 1, 3, 0), "differ-build"),
    (_pack_calver(26, 1, 3, 9), _pack_calver(26, 1, 4, 9), "differ-patch"),
    (_pack_calver(26, 1, 3, 9), _pack_calver(26, 2, 3, 9), "differ-month"),
    (_pack_calver(26, 1, 3, 9), _pack_calver(27, 1, 3, 9), "differ-year"),
    (_pack_calver(0, 0, 0, 0),  _pack_calver(0, 0, 0, 1),  "0 < build=1"),
    (_pack_calver(25, 12, 9, 9999), _pack_calver(26, 0, 0, 0), "year-roll"),
    (_pack_calver(1, 0, 0, 0),  _pack_calver(0, 99, 99, 9999), "year>everything"),
]

# array<size_t,3> values for the sibling toString/isSet overloads.
ARR3_INPUTS: list[tuple[bytes, str]] = [
    (_pack_arr3(0, 0, 0),       "zero"),
    (_pack_arr3(26, 1, 3),      "26.1.3"),
    (_pack_arr3(0, 0, 1),       "0.0.1"),
    (_pack_arr3(1, 0, 0),       "1.0.0"),
    (_pack_arr3(0, 1, 0),       "0.1.0"),
    (_pack_arr3(99, 99, 99),    "99.99.99"),
    (_pack_arr3(0xFFFFFFFFFFFFFFFF, 0, 0), "max,0,0"),
    (_pack_arr3(0, 0xFFFFFFFFFFFFFFFF, 0), "0,max,0"),
    (_pack_arr3(0, 0, 0xFFFFFFFFFFFFFFFF), "0,0,max"),
]

# uint32 inputs for fromDecimal(uint32_t) / fromBinary(uint32_t).
# Cover zero, common LabOne encodings, and bit/decimal-domain edges.
# Non-throwing inputs for `CalVer::CalVer(string const&)`.  The ctor
# routes empty input to a zero-blob short-circuit and otherwise calls
# `extractVersionTriple` (and `boost::lexical_cast<size_t>` for the
# build component when 4 dots are present); both throw on
# non-numeric or out-of-range parts.  Restrict here to inputs that
# parse cleanly so the harness can compare blob outputs without an
# exception trampoline.
CTOR_STRING_INPUTS: list[tuple[bytes, str]] = [
    (b"",               "empty"),
    (b"0.0.0",          "0.0.0"),
    (b"1.2.3",          "1.2.3"),
    (b"26.1.3",         "26.1.3"),
    (b"0.0.0.0",        "0.0.0.0"),
    (b"1.2.3.4",        "1.2.3.4"),
    (b"26.1.3.9",       "current LabOne 4-comp"),
    (b"99.12.31.999",   "large 4-comp"),
    (b"100.200.300",    "3-digit fields"),
]


U32_INPUTS: list[tuple[int, str]] = [
    (0,                   "zero"),
    (1,                   "1"),
    (26010309,            "asDecimal(26.01.3.9)"),  # 26*1e7+1*1e5+3*1e4+9
    (25120000,            "asDecimal(25.12.0.0)"),
    (99999999,            "all-9s decimal"),
    (0xFFFFFFFF,          "u32-max"),
    (0x1A011009,          "asBinary(26.01.3.9)"),   # year=0x1a, month=01, patch=1, build=9
    (0x00000001,          "binary-build-only"),
    (0x00001000,          "binary-patch-only"),
    (0x00010000,          "binary-month-only"),
    (0x01000000,          "binary-year-only"),
    (0xFF00F000,          "binary-fields-mixed"),
    (10000,               "decimal-build-rollover"),
    (10000000,            "decimal-year=1"),
]

# Maps each blob-input symbol name to the corpus to draw from.
# Single-arg blob symbols only.
BLOB_CORPUS: dict[str, str] = {
    "CalVer::year":      "calver",
    "CalVer::month":     "calver",
    "CalVer::patch":     "calver",
    "CalVer::build":     "calver",
    "asDecimal":         "calver",
    "asBinary":          "calver",
    "isSet(CalVer)":     "calver",
    "isSet(array)":      "arr3",
    "toString(CalVer)":  "calver",
    "toString(array)":   "arr3",
    "CalVer::triple":    "calver",
}

# generateSfc: only the MF family reaches the happy path; other families
# raise (and their throw paths are out of scope at E2c — see TODO.md).
# Curated (devType, options) pairs cover empty options, single options,
# multi-option lists, unknown options (silently swallowed), and the
# four MF devType variants {"MF", "MFI", "MFLI", "MFIA"}.
GENERATESFC_INPUTS: list[tuple[bytes, bytes]] = [
    (b"MF",   b""),
    (b"MFI",  b""),
    (b"MFLI", b""),
    (b"MFIA", b""),
    (b"MFLI", b"F5M"),
    (b"MFLI", b"MD"),
    (b"MFLI", b"PID"),
    (b"MFLI", b"DIG"),
    (b"MFLI", b"F5M,MD"),
    (b"MFLI", b"F5M,MD,PID"),
    (b"MFLI", b"F5M,MD,PID,DIG"),
    (b"MFLI", b"PID,F5M,MD"),         # ordering should not matter
    (b"MFLI", b"UNKNOWN"),             # swallowed by the catch-all
    (b"MFLI", b"F5M,UNKNOWN,MD"),      # mixed valid + unknown
    (b"MFLI", b","),                   # empty token
    (b"MFLI", b"F5M,"),                # trailing separator
    (b"MFLI", b"PLL,MOD,RT"),
]


# ---------------------------------------------------------------- #
# Test driver.
# ---------------------------------------------------------------- #

def call_inplace(fn: Callable, data: bytes) -> bytes:
    """Invoke an in-place mutator on a fresh libc++ string and
    return its post-call bytes."""
    p = make_string(data)
    try:
        fn(p)
        return string_bytes(p)
    finally:
        free_string(p)

def call_inplace_n(fn: Callable, data: bytes, n: int) -> bytes:
    p = make_string(data)
    try:
        fn(p, n)
        return string_bytes(p)
    finally:
        free_string(p)


def call_sret_cstr(fn: Callable, cstr: bytes | None) -> bytes:
    """Call `string f(const char*)`: void(sret, c_char_p)."""
    slot = alloc_uninit_slot()
    try:
        fn(slot, cstr)              # ctypes converts None -> NULL
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)

def call_sret_cref(fn: Callable, data: bytes) -> bytes:
    """Call `string f(const string&)`: void(sret, string*)."""
    arg = make_string(data)
    slot = alloc_uninit_slot()
    try:
        fn(slot, arg)
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)
        free_string(arg)

def call_sret_byval(fn: Callable, data: bytes) -> bytes:
    """Call `string f(string)`: void(sret, string*).

    The Itanium ABI passes a non-trivially-copyable class type by
    value as a pointer to caller-constructed storage; the **callee**
    runs the destructor.  So we allocate the argument with the shim
    (placement-new'd `std::string`), pass its pointer, and DO NOT
    free it after the call — only free the underlying raw allocation.
    Since `diff_unreachable_string_make` uses `new std::string(...)`
    we have to skip the `delete` and just `::operator delete` the
    raw block; but ctypes can't easily do that without another shim
    helper.  Practical workaround: use the alloc_uninit slot for the
    argument too, placement-construct via make-then-move... too
    fiddly.  Simpler: re-alloc-uninit the argument and let the
    callee destroy in place, then free the raw 24 B.
    """
    arg_slot = alloc_uninit_slot()
    # Placement-construct the argument string into arg_slot via an
    # extra shim helper:
    _place_construct(arg_slot, data, len(data))
    slot = alloc_uninit_slot()
    try:
        fn(slot, arg_slot)
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)
        # arg_slot was destroyed by the callee; just free the raw 24 B.
        _free_uninit(arg_slot)


def call_sret_cref3(fn: Callable, a: bytes, b: bytes, c: bytes) -> bytes:
    """Call `string f(const string&, const string&, const string&)`."""
    pa = make_string(a)
    pb = make_string(b)
    pc = make_string(c)
    slot = alloc_uninit_slot()
    try:
        fn(slot, pa, pb, pc)
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)
        free_string(pc)
        free_string(pb)
        free_string(pa)


def call_pod_u32_cref(fn: Callable, data: bytes) -> int:
    """Call `uint32_t f(const string&)`: the value comes back in
    %eax (no sret slot)."""
    arg = make_string(data)
    try:
        return int(fn(arg)) & 0xffffffff
    finally:
        free_string(arg)


def call_pod_u64_cref2(fn_addr: int, a: bytes, b: bytes) -> tuple[int | None, str | None]:
    """Call `uint64_t f(const string&, const string&)` via the
    exception-safe trampoline.  Returns `(value, None)` on success
    or `(None, what)` if the callee threw.

    `fn_addr` is the raw function-pointer address (an integer);
    the trampoline casts it back to the right shape internally."""
    pa = make_string(a)
    pb = make_string(b)
    out = ctypes.c_uint64(0)
    buf = ctypes.create_string_buffer(256)
    try:
        rc = _try_pod_u64_cref2(fn_addr, pa, pb,
                                ctypes.byref(out), buf, len(buf))
        if rc == 0:
            return (int(out.value) & 0xffffffffffffffff, None)
        return (None, buf.value.decode("utf-8", errors="replace"))
    finally:
        free_string(pb)
        free_string(pa)


def call_pod_u64_blob(fn: Callable, blob: bytes) -> int:
    """Call `size_t f(Blob const&)` (1 blob arg, u64 return in %rax)."""
    buf = ctypes.create_string_buffer(blob, len(blob))
    return int(fn(ctypes.cast(buf, ctypes.c_void_p))) & 0xffffffffffffffff

def call_pod_u32_blob(fn: Callable, blob: bytes) -> int:
    """Call `uint32_t f(Blob const&)` (1 blob arg, u32 return in %eax)."""
    buf = ctypes.create_string_buffer(blob, len(blob))
    return int(fn(ctypes.cast(buf, ctypes.c_void_p))) & 0xffffffff

def call_pod_bool_blob(fn: Callable, blob: bytes) -> int:
    """Call `bool f(Blob const&)` (1 blob arg, bool return in %al)."""
    buf = ctypes.create_string_buffer(blob, len(blob))
    return int(fn(ctypes.cast(buf, ctypes.c_void_p))) & 0xff

def call_pod_bool_blob2(fn: Callable, a: bytes, b: bytes) -> int:
    """Call `bool f(Blob const&, Blob const&)` (2 blob args)."""
    ba = ctypes.create_string_buffer(a, len(a))
    bb = ctypes.create_string_buffer(b, len(b))
    return int(fn(ctypes.cast(ba, ctypes.c_void_p),
                  ctypes.cast(bb, ctypes.c_void_p))) & 0xff


def call_sret_void(fn: Callable) -> bytes:
    """Call `string f()` (no args, sret string return)."""
    slot = alloc_uninit_slot()
    try:
        fn(slot)
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)


def call_sret_blob_void(fn: Callable, blob_size: int) -> bytes:
    """Call `Blob f()` (no args, sret POD blob return).

    The Itanium ABI passes the return-value pointer in %rdi.  We
    allocate `blob_size` bytes of zeroed scratch, hand its address to
    the callee, and return its post-call contents."""
    buf = ctypes.create_string_buffer(blob_size)
    fn(ctypes.cast(buf, ctypes.c_void_p))
    return ctypes.string_at(buf, blob_size)


def call_sret_blob(fn: Callable, blob: bytes) -> bytes:
    """Call `string f(Blob const&)` (1 blob arg, sret string return).

    sret pointer in %rdi, blob pointer in %rsi."""
    buf = ctypes.create_string_buffer(blob, len(blob))
    slot = alloc_uninit_slot()
    try:
        fn(slot, ctypes.cast(buf, ctypes.c_void_p))
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)


def call_sret_blob_u32(fn: Callable, val: int, blob_size: int) -> bytes:
    """Call `Blob f(uint32_t)` (sret POD blob from uint32 arg).

    sret pointer in %rdi, value in %esi."""
    buf = ctypes.create_string_buffer(blob_size)
    fn(ctypes.cast(buf, ctypes.c_void_p), ctypes.c_uint32(val))
    return ctypes.string_at(buf, blob_size)


def call_ctor_blob_cref(fn: Callable, blob_size: int, data: bytes) -> bytes:
    """Call `void T::T(string const&)` (Itanium C2 base ctor):
    void(this*, string const*).

    sret-style: the *this* slot is allocated zeroed by the caller in
    %rdi, the string argument pointer goes in %rsi, and the callee
    writes its fields into the slot.  Caller owns destruction; for
    POD-only types like CalVer no destructor call is needed."""
    arg = make_string(data)
    buf = ctypes.create_string_buffer(blob_size)
    try:
        fn(ctypes.cast(buf, ctypes.c_void_p), arg)
        return ctypes.string_at(buf, blob_size)
    finally:
        free_string(arg)


def call_ref_blob(fn: Callable, blob: bytes) -> int:
    """Call `Blob const& m(Blob const*)` (member that returns a
    reference; in particular the identity case where %rax = %rdi).

    Returns the offset of the returned pointer from the input
    pointer, so a 0 result means "returned the input pointer
    unchanged".  Comparing offsets (rather than absolute addresses)
    keeps the result independent of allocator placement."""
    buf = ctypes.create_string_buffer(blob, len(blob))
    in_addr = ctypes.cast(buf, ctypes.c_void_p).value or 0
    out_addr = int(fn(ctypes.cast(buf, ctypes.c_void_p)) or 0)
    return out_addr - in_addr


def iter_inputs(sym: Symbol):
    """Yield (call_args_tuple, label).  call_args_tuple matches the
    parameters of the per-kind call_* helper."""
    if sym.kind == "sret_cstr":
        for data in TOCHECKEDSTRING_INPUTS:
            yield ((data,), "NULL" if data is None else _label(data))
        return
    if sym.kind == "sret_cref3":
        for triple in REPLACEUNIT_INPUTS:
            yield (triple, f"({_label(triple[0])}, {_label(triple[1])}, {_label(triple[2])})")
        return
    if sym.kind == "pod_u64_cref2":
        for pair in GENERATESFC_INPUTS:
            yield (pair, f"({_label(pair[0])}, {_label(pair[1])})")
        return
    if sym.kind in ("pod_u64_blob", "pod_u32_blob", "pod_bool_blob",
                    "sret_blob", "ref_blob"):
        # Single-arg blob input drawn from CALVER_INPUTS or ARR3_INPUTS
        # per BLOB_CORPUS.
        corpus = {
            "calver": CALVER_INPUTS,
            "arr3":   ARR3_INPUTS,
        }[BLOB_CORPUS[sym.name]]
        for blob, label in corpus:
            yield ((blob,), label)
        return
    if sym.kind == "pod_bool_blob2":
        for a, b, label in CALVER_PAIRS:
            yield ((a, b), label)
        return
    if sym.kind == "sret_void":
        # Zero-arg sret string return: only one "input" — call once.
        yield ((), "()")
        return
    if sym.kind == "sret_blob_void":
        # Zero-arg sret POD-blob return: only one "input" — call once.
        yield ((), "()")
        return
    if sym.kind == "sret_blob_u32":
        # uint32_t arg, sret POD-blob return.
        for val, label in U32_INPUTS:
            yield ((val,), label)
        return
    if sym.kind == "ctor_blob_cref":
        for data, label in CTOR_STRING_INPUTS:
            yield ((data,), label)
        return
    inputs = list(GENERIC_INPUTS) + PER_SYMBOL_EXTRA.get(sym.name, [])
    if sym.kind == "inplace":
        for data in inputs:
            yield ((data,), _label(data))
    elif sym.kind == "inplace_n":
        for data in inputs:
            for n in TRUNCATE_LIMITS:
                yield ((data, n), f"{_label(data)}|n={n}")
    elif sym.kind in ("sret_cref", "sret_byval"):
        for data in inputs:
            yield ((data,), _label(data))
    elif sym.kind == "pod_u32_cref":
        for data in inputs:
            yield ((data,), _label(data))
    else:
        raise ValueError(f"unknown kind: {sym.kind}")
def _label(b: bytes) -> str:
    if len(b) > 24:
        return f"{b[:20]!r}...({len(b)}B)"
    return repr(b)


def run_symbol(sym: Symbol) -> tuple[int, int]:
    """Run all inputs for one symbol; return (passed, failed)."""
    if sym.kind == "inplace":
        proto_args = [ctypes.c_void_p]
    elif sym.kind == "inplace_n":
        proto_args = [ctypes.c_void_p, ctypes.c_size_t]
    elif sym.kind == "sret_cstr":
        proto_args = [ctypes.c_void_p, ctypes.c_char_p]
    elif sym.kind in ("sret_cref", "sret_byval"):
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "sret_cref3":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p,
                      ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "pod_u32_cref":
        proto_args = [ctypes.c_void_p]
    elif sym.kind == "pod_u64_cref2":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind in ("pod_u64_blob", "pod_u32_blob", "pod_bool_blob"):
        proto_args = [ctypes.c_void_p]
    elif sym.kind == "pod_bool_blob2":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "sret_void":
        proto_args = [ctypes.c_void_p]
    elif sym.kind == "sret_blob_void":
        proto_args = [ctypes.c_void_p]
    elif sym.kind == "sret_blob":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "sret_blob_u32":
        proto_args = [ctypes.c_void_p, ctypes.c_uint32]
    elif sym.kind == "ref_blob":
        proto_args = [ctypes.c_void_p]
    elif sym.kind == "ctor_blob_cref":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    else:
        raise ValueError(f"unknown kind: {sym.kind}")

    fn_orig = orig_fn(sym.orig_offset, None, proto_args)
    fn_cand = cand_fn(sym.cand_mangled, None, proto_args)
    if sym.kind == "pod_u32_cref":
        # Override the void return with uint32_t for both sides.
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint32, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint32, proto_args)
    elif sym.kind == "pod_u64_blob":
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint64, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint64, proto_args)
    elif sym.kind == "pod_u32_blob":
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint32, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint32, proto_args)
    elif sym.kind in ("pod_bool_blob", "pod_bool_blob2"):
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint8, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint8, proto_args)
    elif sym.kind == "ref_blob":
        # Returns a pointer (Blob const&); model as c_void_p for
        # raw-address comparison.
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_void_p, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_void_p, proto_args)
    elif sym.kind == "pod_u64_cref2":
        # The trampoline takes raw function-pointer addresses, not
        # ctypes callables.  Resolve both sides to integer addresses.
        fn_orig = ORIG_BASE + sym.orig_offset                 # type: ignore[assignment]
        fn_cand = ctypes.cast(getattr(_cand, sym.cand_mangled),
                              ctypes.c_void_p).value or 0    # type: ignore[assignment]

    passed = failed = 0
    for args, label in iter_inputs(sym):
        if sym.kind == "inplace":
            data: bytes = args[0]  # type: ignore[assignment]
            o = call_inplace(fn_orig, data)
            c = call_inplace(fn_cand, data)
        elif sym.kind == "inplace_n":
            data2: bytes = args[0]  # type: ignore[assignment]
            n: int     = args[1]    # type: ignore[assignment,misc]
            o = call_inplace_n(fn_orig, data2, n)
            c = call_inplace_n(fn_cand, data2, n)
        elif sym.kind == "sret_cstr":
            cstr = args[0]
            o = call_sret_cstr(fn_orig, cstr)
            c = call_sret_cstr(fn_cand, cstr)
        elif sym.kind == "sret_cref":
            data3: bytes = args[0]  # type: ignore[assignment]
            o = call_sret_cref(fn_orig, data3)
            c = call_sret_cref(fn_cand, data3)
        elif sym.kind == "sret_byval":
            data4: bytes = args[0]  # type: ignore[assignment]
            o = call_sret_byval(fn_orig, data4)
            c = call_sret_byval(fn_cand, data4)
        elif sym.kind == "sret_cref3":
            a3: bytes = args[0]  # type: ignore[assignment]
            b3: bytes = args[1]  # type: ignore[assignment]
            c3: bytes = args[2]  # type: ignore[assignment]
            o = call_sret_cref3(fn_orig, a3, b3, c3)
            c = call_sret_cref3(fn_cand, a3, b3, c3)
        elif sym.kind == "pod_u32_cref":
            data5: bytes = args[0]  # type: ignore[assignment]
            o = call_pod_u32_cref(fn_orig, data5)
            c = call_pod_u32_cref(fn_cand, data5)
        elif sym.kind == "pod_u64_blob":
            blob: bytes = args[0]  # type: ignore[assignment]
            o = call_pod_u64_blob(fn_orig, blob)
            c = call_pod_u64_blob(fn_cand, blob)
        elif sym.kind == "pod_u32_blob":
            blob2: bytes = args[0]  # type: ignore[assignment]
            o = call_pod_u32_blob(fn_orig, blob2)
            c = call_pod_u32_blob(fn_cand, blob2)
        elif sym.kind == "pod_bool_blob":
            blob3: bytes = args[0]  # type: ignore[assignment]
            o = call_pod_bool_blob(fn_orig, blob3)
            c = call_pod_bool_blob(fn_cand, blob3)
        elif sym.kind == "pod_bool_blob2":
            ba2: bytes = args[0]  # type: ignore[assignment]
            bb2: bytes = args[1]  # type: ignore[assignment]
            o = call_pod_bool_blob2(fn_orig, ba2, bb2)
            c = call_pod_bool_blob2(fn_cand, ba2, bb2)
        elif sym.kind == "sret_void":
            o = call_sret_void(fn_orig)
            c = call_sret_void(fn_cand)
        elif sym.kind == "sret_blob_void":
            # CalVer is 32 B (4 x size_t).
            o = call_sret_blob_void(fn_orig, 32)
            c = call_sret_blob_void(fn_cand, 32)
        elif sym.kind == "sret_blob":
            blob4: bytes = args[0]  # type: ignore[assignment]
            o = call_sret_blob(fn_orig, blob4)
            c = call_sret_blob(fn_cand, blob4)
        elif sym.kind == "sret_blob_u32":
            val32: int = args[0]    # type: ignore[assignment]
            o = call_sret_blob_u32(fn_orig, val32, 32)  # CalVer = 32 B
            c = call_sret_blob_u32(fn_cand, val32, 32)
        elif sym.kind == "ref_blob":
            blob5: bytes = args[0]  # type: ignore[assignment]
            o = call_ref_blob(fn_orig, blob5)
            c = call_ref_blob(fn_cand, blob5)
        elif sym.kind == "ctor_blob_cref":
            data6: bytes = args[0]  # type: ignore[assignment]
            o = call_ctor_blob_cref(fn_orig, 32, data6)  # CalVer = 32 B
            c = call_ctor_blob_cref(fn_cand, 32, data6)
        elif sym.kind == "pod_u64_cref2":
            a6: bytes = args[0]  # type: ignore[assignment]
            b6: bytes = args[1]  # type: ignore[assignment]
            o = call_pod_u64_cref2(fn_orig, a6, b6)  # type: ignore[arg-type]
            c = call_pod_u64_cref2(fn_cand, a6, b6)  # type: ignore[arg-type]
            # When both sides throw, compare only the *fact* of throw,
            # not the exception text: RTTI for `zhinst::Exception`
            # differs between the original (statically-linked,
            # hidden-visibility libc++) and the candidate (dynamically-
            # linked libc++), so the original's throw escapes the
            # trampoline's `catch(const std::exception&)` and is
            # caught by `catch(...)` as "<unknown>", while the
            # candidate's throw is caught with full `what()`.  This
            # is a harness limitation, not a behavioural divergence.
            if o[0] is None and c[0] is None:
                # Both threw — collapse to a sentinel pair.
                o = (None, "<threw>")
                c = (None, "<threw>")
        else:
            raise ValueError(f"unknown kind: {sym.kind}")

        ok = o == c
        if ok:
            passed += 1
        else:
            failed += 1
            print(f"  {RED('FAIL')} {sym.name}({label})")
            print(f"    orig: {o!r}")
            print(f"    cand: {c!r}")
    status = GREEN("PASS") if failed == 0 else RED(f"FAIL ({failed})")
    total = passed + failed
    print(f"{status} {BOLD(sym.name)} {DIM(f'({passed}/{total})')}")
    return passed, failed


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--filter", default=None,
                    help="substring to match against symbol name")
    args = ap.parse_args()

    syms = SYMBOLS
    if args.filter:
        syms = [s for s in syms if args.filter in s.name]
        if not syms:
            print(RED(f"no symbols matched filter '{args.filter}'"),
                  file=sys.stderr)
            return 2

    print(DIM(f"original  : {ORIGINAL_SO} (text base 0x{ORIG_BASE:x})"))
    print(DIM(f"candidate : {CANDIDATE_SO}"))
    print()

    total_p = total_f = 0
    for sym in syms:
        p, f = run_symbol(sym)
        total_p += p
        total_f += f

    print()
    if total_f == 0:
        print(GREEN(f"OK: {total_p} cases passed"))
        return 0
    print(RED(f"FAIL: {total_f} of {total_p + total_f} cases failed"))
    return 1


if __name__ == "__main__":
    sys.exit(main())
