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


@dataclass(frozen=True)
class Symbol:
    name: str
    orig_offset: int
    cand_mangled: str
    # 'inplace' = void(string&); 'inplace_n' = void(string&, size_t)
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
}

TRUNCATE_LIMITS = [0, 1, 2, 3, 5, 7, 10, 22, 23, 100]


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


def iter_inputs(sym: Symbol):
    """Yield (call_args_tuple, label).  call_args_tuple matches the
    parameters of `call_inplace` / `call_inplace_n`."""
    inputs = list(GENERIC_INPUTS) + PER_SYMBOL_EXTRA.get(sym.name, [])
    if sym.kind == "inplace":
        for data in inputs:
            yield ((data,), _label(data))
    elif sym.kind == "inplace_n":
        for data in inputs:
            for n in TRUNCATE_LIMITS:
                yield ((data, n), f"{_label(data)}|n={n}")

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
    else:
        raise ValueError(f"unknown kind: {sym.kind}")

    fn_orig = orig_fn(sym.orig_offset, None, proto_args)
    fn_cand = cand_fn(sym.cand_mangled, None, proto_args)

    passed = failed = 0
    for args, label in iter_inputs(sym):
        if sym.kind == "inplace":
            data: bytes = args[0]  # type: ignore[assignment]
            o = call_inplace(fn_orig, data)
            c = call_inplace(fn_cand, data)
        else:
            data2: bytes = args[0]  # type: ignore[assignment]
            n: int     = args[1]    # type: ignore[assignment,misc]
            o = call_inplace_n(fn_orig, data2, n)
            c = call_inplace_n(fn_cand, data2, n)

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
