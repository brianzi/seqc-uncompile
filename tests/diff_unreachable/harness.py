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
import struct
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

_try_sret_blob_cref = _cand.diff_unreachable_try_sret_blob_cref
_try_sret_blob_cref.restype = ctypes.c_int
_try_sret_blob_cref.argtypes = [
    ctypes.c_void_p,  # fn pointer
    ctypes.c_void_p,  # void* out_blob
    ctypes.c_void_p,  # const string* arg
    ctypes.c_char_p,  # what buffer
    ctypes.c_size_t,  # what cap
]

# ostream helpers — for `cref_ostream_throws` shape (e.g. op<<(ostream&,
# CalVer const&)).  Both sides link the same libc++, so a single
# ostringstream constructed here can be passed to either.
_oss_make = _cand.diff_unreachable_ostringstream_make
_oss_make.restype = ctypes.c_void_p
_oss_make.argtypes = []
_oss_free = _cand.diff_unreachable_ostringstream_free
_oss_free.restype = None
_oss_free.argtypes = [ctypes.c_void_p]
_oss_as_ostream = _cand.diff_unreachable_ostringstream_as_ostream
_oss_as_ostream.restype = ctypes.c_void_p
_oss_as_ostream.argtypes = [ctypes.c_void_p]
_oss_str = _cand.diff_unreachable_ostringstream_str
_oss_str.restype = ctypes.c_size_t
_oss_str.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]

_try_cref_ostream = _cand.diff_unreachable_try_cref_ostream
_try_cref_ostream.restype = ctypes.c_int
_try_cref_ostream.argtypes = [
    ctypes.c_void_p,  # fn pointer
    ctypes.c_void_p,  # ostream* (upcast)
    ctypes.c_void_p,  # const Blob* arg
    ctypes.c_char_p,  # what buffer
    ctypes.c_size_t,  # what cap
]

# DeviceType / AwgDeviceProps helpers — for the `sret_props_cref`
# shape used by the `getAwgDeviceProps<T>` family.  Both sides link
# the same libc++ AND the same DeviceType / AwgDeviceProps machinery
# (the relevant ctors and dtors live in the same TU as the targets),
# so a single instance constructed here can be passed safely to
# either.
_devicetype_make = _cand.diff_unreachable_devicetype_make
_devicetype_make.restype = ctypes.c_void_p
_devicetype_make.argtypes = [ctypes.c_char_p, ctypes.c_size_t,
                             ctypes.c_char_p, ctypes.c_size_t]
_devicetype_free = _cand.diff_unreachable_devicetype_free
_devicetype_free.restype = None
_devicetype_free.argtypes = [ctypes.c_void_p]

_awgprops_alloc_uninit = _cand.diff_unreachable_awgdeviceprops_alloc_uninit
_awgprops_alloc_uninit.restype = ctypes.c_void_p
_awgprops_alloc_uninit.argtypes = []
_awgprops_free_uninit = _cand.diff_unreachable_awgdeviceprops_free_uninit
_awgprops_free_uninit.restype = None
_awgprops_free_uninit.argtypes = [ctypes.c_void_p]
_awgprops_destroy_in_place = _cand.diff_unreachable_awgdeviceprops_destroy_in_place
_awgprops_destroy_in_place.restype = None
_awgprops_destroy_in_place.argtypes = [ctypes.c_void_p]

_vec_u32_alloc_uninit = _cand.diff_unreachable_vec_u32_alloc_uninit
_vec_u32_alloc_uninit.restype = ctypes.c_void_p
_vec_u32_alloc_uninit.argtypes = []
_vec_u32_free_uninit = _cand.diff_unreachable_vec_u32_free_uninit
_vec_u32_free_uninit.restype = None
_vec_u32_free_uninit.argtypes = [ctypes.c_void_p]
_vec_u32_destroy_in_place = _cand.diff_unreachable_vec_u32_destroy_in_place
_vec_u32_destroy_in_place.restype = None
_vec_u32_destroy_in_place.argtypes = [ctypes.c_void_p]
_vec_u32_data = _cand.diff_unreachable_vec_u32_data
_vec_u32_data.restype = ctypes.c_void_p
_vec_u32_data.argtypes = [ctypes.c_void_p]
_vec_u32_size = _cand.diff_unreachable_vec_u32_size
_vec_u32_size.restype = ctypes.c_size_t
_vec_u32_size.argtypes = [ctypes.c_void_p]
_vec_u32_make = _cand.diff_unreachable_vec_u32_make
_vec_u32_make.restype = ctypes.c_void_p
_vec_u32_make.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
_vec_u32_free = _cand.diff_unreachable_vec_u32_free
_vec_u32_free.restype = None
_vec_u32_free.argtypes = [ctypes.c_void_p]


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
    # Throwing sret-blob-from-string-cref shapes.  Use the
    # exception-safe trampoline to compare both sides on inputs that
    # may throw `boost::bad_lexical_cast`; both-threw outcomes collapse
    # to a sentinel pair.
    # CalVer fromDecimal(string const&) — sret 32-byte CalVer.
    Symbol("fromDecimal(string)",    0x100520,
           "_ZN6zhinst11fromDecimalERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_blob_cref_throws"),
    # array<size_t,3> extractVersionTriple(string const&) — sret 24-byte array.
    Symbol("extractVersionTriple",   0x101570,
           "_ZN6zhinst20extractVersionTripleERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
           "sret_blob_cref_throws"),
    # std::ostream& operator<<(std::ostream&, CalVer const&) — captures
    # toString(CalVer) output via a libc++ ostringstream.
    Symbol("operator<<(CalVer)",     0x100b40,
           "_ZN6zhinstlsERNSt3__113basic_ostreamIcNS0_11char_traitsIcEEEERKNS_6CalVerE",
           "cref_ostream_throws"),
    # ---- Phase G: device/awg_device_props.cpp ----
    # AwgDeviceType toAwgDeviceType(DeviceTypeCode, AwgSequencerType) — POD u32 return,
    # two POD u32 args.
    Symbol("toAwgDeviceType",        0x2cbd60,
           "_ZN6zhinst15toAwgDeviceTypeENS_14DeviceTypeCodeENS_16AwgSequencerTypeE",
           "pod_u32_2u32"),
    # std::string toString(AwgSequencerType) — sret string, single POD u32 arg.
    Symbol("toString(AwgSequencerType)", 0x2cbce0,
           "_ZN6zhinst8toStringENS_16AwgSequencerTypeE",
           "sret_str_u32"),
    # std::string makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode, AwgSequencerType) —
    # sret string, two POD u32 args.
    Symbol("makeUnsupportedAwgSequencerErrorMessage", 0x2cbdd0,
           "_ZN6zhinst39makeUnsupportedAwgSequencerErrorMessageENS_14DeviceTypeCodeENS_16AwgSequencerTypeE",
           "sret_str_2u32"),
    # ---- Phase G: getAwgDeviceProps<T> family (9 specializations) ----
    # Signature: AwgDeviceProps getAwgDeviceProps<T>(DeviceType const&).
    # Only HDAWG actually consults `dt` (calls dt.hasOption(ME)); the
    # others ignore it.  All 9 wired with the `sret_props_cref` shape.
    Symbol("getAwgDeviceProps<UHFLI>",    0x2cc900,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE1EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<HDAWG>",    0x2ccb80,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE2EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<UHFQA>",    0x2cc5f0,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE4EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<SHFQA>",    0x2cce30,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE8EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<SHFSG>",    0x2cd0c0,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE16EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<SHFQC_SG>", 0x2cd350,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE32EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<SHFLI>",    0x2cd5e0,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE64EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<VHFLI>",    0x2cd870,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE256EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    Symbol("getAwgDeviceProps<GHFLI>",    0x2cdb00,
           "_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE128EEENS_14AwgDevicePropsERKNS_10DeviceTypeE",
           "sret_props_cref"),
    # ZiFolder::folderPath(string const&, string const&) const
    # ABI: void(sret, this*, string*, string*).  this* is a ZiFolder
    # which is layout-equivalent to a single std::string at offset 0
    # (basePath_).  Per IF-272 the recon body diverged from the
    # binary on the "/data"/"/settings" subdir-length branch; the
    # harness exists primarily to prevent regression of that fix.
    Symbol("ZiFolder::folderPath",        0x2ce2f0,
           "_ZNK6zhinst8ZiFolder10folderPathERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_",
           "sret_zifolder_2cref"),
    # util::wave::hash(string const&) — SHA-1 of file contents at the
    # given path.  Returns vector<unsigned int> by sret (24-byte slot).
    # Exercises file I/O + boost::uuids::detail::sha1 pipeline plus the
    # open-fail SHA-1-IV branch.
    Symbol("util::wave::hash",            0x299760,
           "_ZN6zhinst4util4wave4hashERKNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE",
           "sret_vec_u32_cref"),
    # util::wave::hash2str(vector<u32> const&) — format SHA-1 digest as
    # lowercase hex.  String sret + vector<u32> by const-ref.
    Symbol("util::wave::hash2str",        0x299d60,
           "_ZN6zhinst4util4wave8hash2strERKNSt3__16vectorIjNS2_9allocatorIjEEEE",
           "sret_str_vec_u32_cref"),
    # util::wave::double2awg(double, uint) — 14-bit signed sample with
    # 2 marker bits in the low 2 bits.  Returns uint16_t in %ax.
    Symbol("util::wave::double2awg",      0x299630,
           "_ZN6zhinst4util4wave10double2awgEdj",
           "pod_u16_double_u32"),
    # util::wave::double2awg1m(double, uint) — 15-bit signed sample
    # with 1 marker bit in the low bit.
    Symbol("util::wave::double2awg1m",    0x299680,
           "_ZN6zhinst4util4wave12double2awg1mEdj",
           "pod_u16_double_u32"),
    # util::wave::double2awg16(double) — 16-bit signed sample, no
    # markers.  NaN propagates via SSE2 maxsd semantics.
    Symbol("util::wave::double2awg16",    0x299700,
           "_ZN6zhinst4util4wave12double2awg16Ed",
           "pod_u16_double"),
    # util::wave::awg2double(u16) — inverse of double2awg: clear marker
    # bits, sign-extend, divide by 32767.0.
    Symbol("util::wave::awg2double",      0x2996d0,
           "_ZN6zhinst4util4wave10awg2doubleEt",
           "pod_double_u16"),
    # util::wave::awg2marker(u16) — extract bottom 2 marker bits.
    Symbol("util::wave::awg2marker",      0x2996f0,
           "_ZN6zhinst4util4wave10awg2markerEt",
           "pod_u8_u16"),
    # util::wave::awg2double16(u32) — inverse of double2awg16: shift
    # right 2, sign-extend, divide by 32767.0.
    Symbol("util::wave::awg2double16",    0x299740,
           "_ZN6zhinst4util4wave12awg2double16Ej",
           "pod_double_u32"),
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
# Mixed valid + throwing inputs for `extractVersionTriple(string)` and
# `fromDecimal(string)`.  Both call `boost::lexical_cast<size_t>` on
# parts of the string and throw on non-numeric / out-of-range parts.
# `fromDecimal(string)` short-circuits empty input to a zero CalVer;
# `extractVersionTriple` does not.  Each input drives both shapes:
# valid inputs produce a blob match, throwing inputs produce a
# both-threw sentinel collapse.
THROW_STRING_INPUTS: list[tuple[bytes, str]] = [
    # ---- non-throwing ----
    (b"",                "empty"),
    (b"0.0.0",           "0.0.0"),
    (b"1.2.3",           "1.2.3"),
    (b"26.1.3",          "current 3-comp"),
    (b"100.200.300",     "3-digit fields"),
    (b"0",               "single 0"),
    (b"42",              "single int"),
    # ---- throwing (lexical_cast<size_t> rejects) ----
    (b"abc",             "non-numeric"),
    (b"1.2.x",           "trailing non-numeric"),
    (b"1..2",            "empty middle"),
    (b".1.2",            "leading dot"),
    (b"-1.2.3",          "negative leading"),
    (b"1.2.3.4.5",       "too many parts"),
]


# Note: prior to the IF-270 fix, `extractVersionTriple` required a
# restricted corpus (`EXTRACT_TRIPLE_INPUTS`) excluding inputs that
# triggered the divergence (orig caught and zeroed; recon threw).
# After the fix the full `THROW_STRING_INPUTS` corpus passes 13/13.


# Per-symbol corpus override for `sret_blob_cref_throws`.  Symbols
# absent from this map fall back to `THROW_STRING_INPUTS`.
# Per-symbol corpus override for `sret_blob_cref_throws`.  Symbols
# absent from this map fall back to `THROW_STRING_INPUTS`.  Currently
# empty: IF-270 (extractVersionTriple's missing outer try/catch and
# wrong token-compress mode) was fixed, so the symbol now passes the
# full corpus.
THROW_CORPUS_OVERRIDE: dict[str, list[tuple[bytes, str]]] = {}


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
    "CalVer::triple":      "calver",
    "operator<<(CalVer)":  "calver",
}

# Per-symbol sret blob size (bytes).  Currently only used by the
# `sret_blob_cref_throws` shape, which would otherwise need a separate
# kind per blob size.
BLOB_SIZE: dict[str, int] = {
    "fromDecimal(string)":   32,   # CalVer
    "extractVersionTriple":  24,   # array<size_t,3>
}

# ---- AWG device-props corpora (Phase G / Task 4) ----
# DeviceTypeCode covers 0..32 plus a couple of out-of-range probes to
# exercise the "unsupported" path; AwgSequencerType is the full {Auto,
# QA, SG} set plus an out-of-range probe.
DEVICE_TYPE_CODES: list[tuple[int, str]] = [
    (0,  "Unknown"), (1,  "HF2"),     (2,  "HF2LI"),  (3,  "HF2IS"),
    (4,  "UHF"),     (5,  "UHFLI"),   (6,  "UHFAWG"), (7,  "UHFQA"),
    (8,  "UHFIA"),   (9,  "MF"),      (10, "MFLI"),   (11, "MFIA"),
    (12, "HDAWG"),   (13, "HDAWG4"),  (14, "HDAWG8"), (15, "SHF"),
    (16, "SHFQA2"),  (17, "SHFQA4"),  (18, "SHFSG2"), (19, "SHFSG4"),
    (20, "SHFSG8"),  (21, "SHFQC"),   (22, "SHFLI"),  (23, "PQSC"),
    (24, "SHFACC"),  (25, "SHFPPC2"), (26, "SHFPPC4"), (27, "GHF"),
    (28, "GHFLI"),   (29, "HWMOCK"),  (30, "QHUB"),   (31, "VHF"),
    (32, "VHFLI"),
    (33, "out-of-range-low"), (0xFFFFFFFF, "u32-max"),
]

AWG_SEQUENCER_TYPES: list[tuple[int, str]] = [
    (0, "Auto"),
    (1, "QA"),
    (2, "SG"),
    (3, "out-of-range"),
]

# Per-symbol DeviceType corpora for `sret_props_cref`.  Most of the 9
# `getAwgDeviceProps<T>` specializations ignore `dt` (the binary
# never reads through the pointer), so a single placeholder "HDAWG8"
# DeviceType is sufficient to exercise the full sret-decode path.
# Only `getAwgDeviceProps<HDAWG>` actually consults `dt` (calls
# `dt.hasOption(ME)` to choose between two `maxElfSize` limits), so
# we feed it both ME-on and ME-off variants.
AWGPROPS_DT_CORPUS: dict[str, list[tuple[bytes, bytes, str]]] = {
    "getAwgDeviceProps<HDAWG>": [
        (b"HDAWG8", b"",   "HDAWG8/no-options"),
        (b"HDAWG8", b"ME", "HDAWG8/ME"),
        (b"HDAWG4", b"",   "HDAWG4/no-options"),
        (b"HDAWG4", b"ME", "HDAWG4/ME"),
    ],
}
# Default corpus for the 8 dt-ignoring specializations.  Use the
# "matching" device-type name when one exists so the DeviceType is
# at least sensible, even though the callee never reads it.
AWGPROPS_DT_DEFAULT: dict[str, list[tuple[bytes, bytes, str]]] = {
    "getAwgDeviceProps<UHFLI>":    [(b"UHFLI",  b"", "UHFLI")],
    "getAwgDeviceProps<UHFQA>":    [(b"UHFQA",  b"", "UHFQA")],
    "getAwgDeviceProps<SHFQA>":    [(b"SHFQA4", b"", "SHFQA4")],
    "getAwgDeviceProps<SHFSG>":    [(b"SHFSG4", b"", "SHFSG4")],
    "getAwgDeviceProps<SHFQC_SG>": [(b"SHFQC",  b"", "SHFQC")],
    "getAwgDeviceProps<SHFLI>":    [(b"SHFLI",  b"", "SHFLI")],
    "getAwgDeviceProps<VHFLI>":    [(b"VHFLI",  b"", "VHFLI")],
    "getAwgDeviceProps<GHFLI>":    [(b"GHFLI",  b"", "GHFLI")],
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


# ZiFolder::folderPath corpus: each entry is (basePath_, subdir, extra,
# label).  basePath_ becomes the ZiFolder's only field; subdir drives
# the three-way branch (`/data` and `/settings` skip the
# `$Zurich Instruments` segment, anything else inserts it); extra is
# appended verbatim if non-empty.  The empty-extra cases verify the
# `result /= extra` skip.
ZIFOLDER_FOLDERPATH_INPUTS: list[tuple[bytes, bytes, bytes, str]] = [
    # The two canonical "vendor-skip" subdirs.
    (b"dev1234", b"/data",      b"",         "/data + empty extra"),
    (b"dev1234", b"/settings",  b"",         "/settings + empty extra"),
    (b"dev1234", b"/data",      b"foo",      "/data + extra"),
    (b"dev1234", b"/settings",  b"bar.json", "/settings + extra"),
    # Non-canonical subdirs of various lengths — must trigger the
    # `$Zurich Instruments` insertion.
    (b"dev1234", b"/cache",     b"",         "/cache (size 6) -> vendor"),
    (b"dev1234", b"/tmp",       b"",         "/tmp (size 4) -> vendor"),
    (b"dev1234", b"/datax",     b"",         "/datax (size 6, similar) -> vendor"),
    (b"dev1234", b"/dat",       b"",         "/dat (size 4, prefix) -> vendor"),
    (b"dev1234", b"/setting",   b"",         "/setting (size 8, near-9) -> vendor"),
    (b"dev1234", b"/settings1", b"",         "/settings1 (size 10) -> vendor"),
    (b"dev1234", b"",           b"",         "empty subdir + empty extra"),
    # Length-9 subdir that is NOT "/settings" — should still get vendor.
    (b"dev1234", b"123456789",  b"",         "9-char non-/settings -> vendor"),
    # Length-5 subdir that is NOT "/data" — should still get vendor.
    (b"dev1234", b"12345",      b"",         "5-char non-/data -> vendor"),
    # Empty basePath_ — exercises the path's empty-string append.
    (b"",        b"/data",      b"",         "empty basePath /data"),
    (b"",        b"/cache",     b"foo",      "empty basePath /cache + extra"),
    # Long basePath_ (forces libc++ long-string layout).
    (b"a" * 32,  b"/data",      b"",         "long basePath /data"),
    (b"a" * 32,  b"/cache",     b"",         "long basePath /cache"),
]


# `util::wave::hash` corpus.  The function reads bytes from a file on
# disk and SHA-1's them.  Each entry is (file_contents_or_None, label):
# None means "do not create the file", which exercises the open-fail
# path that returns the SHA-1 IV.  A sentinel "<missing>" label is
# tagged below.
#
# Boundary inputs cover the 1024-byte chunked-read loop:
#   - empty file (0 chunks)
#   - <1024 B (1 partial chunk)
#   - exactly 1024 B (1 full chunk + EOF)
#   - exactly 1025 B (1 full + 1 1-byte chunk)
#   - >>1024 B multi-chunk
#   - exactly 64 B (matches boost's internal SHA-1 block size)
WAVE_HASH_INPUTS: list[tuple[bytes | None, str]] = [
    (None,            "<missing-file>"),
    (b"",             "empty file"),
    (b"a",            "1 byte"),
    (b"hello",        "5 bytes"),
    (b"a" * 63,       "63 bytes (just under SHA-1 block)"),
    (b"a" * 64,       "64 bytes (exactly SHA-1 block)"),
    (b"a" * 65,       "65 bytes"),
    (b"a" * 1023,     "1023 bytes (just under chunk)"),
    (b"a" * 1024,     "1024 bytes (exactly one chunk)"),
    (b"a" * 1025,     "1025 bytes (one chunk + 1)"),
    (b"a" * 4096,     "4096 bytes (4 chunks)"),
    (b"\x00" * 100,   "100 NULs"),
    (b"\xff" * 100,   "100 0xFF bytes"),
    (bytes(range(256)), "0..255 bytes"),
]


# `util::wave::hash2str` corpus.  Inputs are vectors of unsigned ints
# (the SHA-1-like 5-word digest is the natural input, but this function
# accepts any vector size).
import math as _math

WAVE_HASH2STR_INPUTS: list[tuple[list[int], str]] = [
    ([],                                        "empty vector"),
    ([0x00000000],                              "single 0"),
    ([0xffffffff],                              "single 0xffffffff"),
    ([0xdeadbeef],                              "single 0xdeadbeef"),
    ([0x00000001],                              "single 1 (leading zero pad)"),
    ([0x12345678, 0x9abcdef0],                  "two words"),
    ([0xda39a3ee, 0x5e6b4b0d, 0x3255bfef,
      0x95601890, 0xafd80709],                  "SHA-1 of empty"),
    ([0x00, 0x00, 0x00, 0x00, 0x00],            "5 zeros"),
    ([0xffffffff] * 5,                          "5 ffs"),
    (list(range(10)),                           "10 small ints"),
]


# `util::wave::double2awg{,1m,16}` corpora.
# Boundary inputs cover: clamp floor (-1.0), saturate threshold (>1.0),
# zero, sub-/sup-normal, NaN/Inf (critical for double2awg16 NaN
# propagation per the SSE2 maxsd binary-faithful path).
DOUBLE2AWG_INPUTS: list[tuple[float, str]] = [
    (-1.5,            "-1.5 (below clamp)"),
    (-1.0,            "-1.0 (clamp floor)"),
    (-0.5,            "-0.5"),
    (-1e-9,           "-1e-9"),
    (0.0,             "0.0"),
    (-0.0,            "-0.0"),
    (1e-9,            "1e-9"),
    (0.25,            "0.25"),
    (0.5,             "0.5"),
    (0.999,           "0.999"),
    (1.0,             "1.0 (boundary)"),
    (1.0 + 1e-9,      "1.0+eps (just over)"),
    (1.5,             "1.5 (over threshold)"),
    (1e9,             "1e9"),
    (_math.inf,       "+inf"),
    (-_math.inf,      "-inf"),
    (_math.nan,       "NaN"),
]

# Marker corpus for the 2-arg double2awg / double2awg1m: covers all
# low-bit patterns (only low 2 / low 1 bits matter; high bits should
# be ignored).
MARKER_INPUTS: list[int] = [0, 1, 2, 3, 0xff, 0xff_ff_ff_ff]

# Inverse-direction corpus for awg2double / awg2marker / awg2double16.
# 16-bit input space is small enough to sample boundary + a few
# representative middle values.
U16_INPUTS: list[tuple[int, str]] = [
    (0x0000, "0x0000"),
    (0x0001, "0x0001 (marker bit only)"),
    (0x0002, "0x0002 (marker bit only)"),
    (0x0003, "0x0003 (both markers)"),
    (0x0004, "0x0004 (sample=1, no marker)"),
    (0x7fff, "0x7fff (max positive area)"),
    (0x8000, "0x8000 (min negative)"),
    (0x8003, "0x8003 (min neg + markers)"),
    (0xfffc, "0xfffc (-1, no marker)"),
    (0xffff, "0xffff (-1 + markers)"),
    (0x1234, "0x1234 (mid)"),
    (0xabcd, "0xabcd (mid)"),
]

# u32 input corpus for awg2double16: sample is in bits [17:2] after
# `shr 2` then sign-extended from 16-bit.
U32_AWG_INPUTS: list[tuple[int, str]] = [
    (0x00000000, "0x00000000"),
    (0x00000004, "0x00000004 (sample=1)"),
    (0x0001fffc, "0x0001fffc (sample=0x7fff)"),
    (0x00020000, "0x00020000 (sample=0x8000)"),
    (0x0003fffc, "0x0003fffc (sample=0xffff = -1)"),
    (0xfffffffc, "0xfffffffc (high bits set)"),
    (0xdeadbeef, "0xdeadbeef"),
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


def call_sret_blob_cref_throws(fn_addr: int, blob_size: int,
                               data: bytes) -> tuple[bytes | None, str | None]:
    """Call `Blob f(const std::string&)` (sret POD blob, may throw) via
    the exception-safe trampoline.  Returns `(blob_bytes, None)` on
    success or `(None, what)` if the callee threw.

    `fn_addr` is the raw function-pointer address (an integer); the
    trampoline casts it back to the right shape internally.  `blob_size`
    is the byte size of the sret return value (CalVer = 32, array<3> =
    24)."""
    arg = make_string(data)
    out_blob = ctypes.create_string_buffer(blob_size)
    what_buf = ctypes.create_string_buffer(256)
    try:
        rc = _try_sret_blob_cref(fn_addr,
                                 ctypes.cast(out_blob, ctypes.c_void_p),
                                 arg, what_buf, len(what_buf))
        if rc == 0:
            return (ctypes.string_at(out_blob, blob_size), None)
        return (None, what_buf.value.decode("utf-8", errors="replace"))
    finally:
        free_string(arg)


def call_cref_ostream_throws(fn_addr: int,
                             blob: bytes) -> tuple[str | None, str | None]:
    """Call `std::ostream& f(std::ostream&, Blob const&)` via the
    exception-safe trampoline; return `(captured_string, None)` on
    success or `(None, what)` if the callee threw.

    A fresh `std::ostringstream` is constructed for each call so the
    captured output reflects only this invocation.  `fn_addr` is the
    raw function-pointer address; the trampoline casts it back to the
    right shape internally."""
    buf = ctypes.create_string_buffer(blob, len(blob))
    oss = _oss_make()
    os = _oss_as_ostream(oss)
    what_buf = ctypes.create_string_buffer(256)
    try:
        rc = _try_cref_ostream(fn_addr,
                               os,
                               ctypes.cast(buf, ctypes.c_void_p),
                               what_buf, len(what_buf))
        if rc == 0:
            out = ctypes.create_string_buffer(4096)
            n = _oss_str(oss, out, len(out))
            return (out.value.decode("utf-8", errors="replace"), None)
        return (None, what_buf.value.decode("utf-8", errors="replace"))
    finally:
        _oss_free(oss)


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


def call_pod_u32_2u32(fn: Callable, a: int, b: int) -> int:
    """Call `uint32_t f(uint32_t, uint32_t)`: both args in %edi/%esi,
    return in %eax.  Used for AwgDeviceType toAwgDeviceType(...)."""
    return int(fn(ctypes.c_uint32(a), ctypes.c_uint32(b))) & 0xffffffff


def call_sret_str_u32(fn: Callable, val: int) -> bytes:
    """Call `string f(uint32_t)`: void(sret, c_uint32).
    sret pointer in %rdi, value in %esi."""
    slot = alloc_uninit_slot()
    try:
        fn(slot, ctypes.c_uint32(val))
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)


def call_sret_str_2u32(fn: Callable, a: int, b: int) -> bytes:
    """Call `string f(uint32_t, uint32_t)`: void(sret, c_uint32, c_uint32).
    sret pointer in %rdi, args in %esi/%edx."""
    slot = alloc_uninit_slot()
    try:
        fn(slot, ctypes.c_uint32(a), ctypes.c_uint32(b))
        return string_bytes(slot)
    finally:
        destroy_and_free_slot(slot)


# AwgDeviceProps layout (libc++; verified in awg_device_props.hpp):
#   +0x00  AwgDeviceType deviceType    (uint32_t one-hot bit; 4 B + 4 B pad)
#   +0x08  std::string   elfDataPattern        (24 B)
#   +0x20  std::string   elfProgressPattern    (24 B)
#   +0x38  std::string   enablePattern         (24 B)
#   +0x50  uint64_t      maxElfSize            (8 B)
#   +0x58  uint32_t      addressImpl           (4 B)
#   +0x5c  uint32_t      sampleFormat          (4 B)
#   +0x60  bool          isHirzel              (1 B + 7 B pad)
#   +0x68  std::string   fpgaRevisionPattern   (24 B)
#   Total: 0x80 bytes.
AWGPROPS_SIZE = 0x80
AWGPROPS_STR_OFFSETS = (0x08, 0x20, 0x38, 0x68)


def make_devicetype(dtype: bytes, opts: bytes) -> int:
    """Heap-allocate a libc++ DeviceType from the (name, options) pair.
    Caller releases via `free_devicetype`."""
    return _devicetype_make(dtype, len(dtype), opts, len(opts))


def free_devicetype(p: int) -> None:
    _devicetype_free(p)


def call_sret_props_cref(
        fn: Callable, dt_ptr: int
) -> tuple[int, bytes, bytes, bytes, int, int, int, int, bytes]:
    """Call `AwgDeviceProps f(DeviceType const&)`: void(sret, DeviceType*).

    sret pointer in %rdi, dt pointer in %rsi.  Decode the 128-byte
    slot into a normalized tuple
    `(deviceType, elfData, elfProgress, enable, maxElfSize,
       addressImpl, sampleFormat, isHirzel, fpgaRevision)`
    so heap-pointer differences in the std::string fields don't
    cause spurious diffs.

    The slot is destroyed via `~AwgDeviceProps()` after read-out and
    the raw 128 B reclaimed via the matching free_uninit helper."""
    slot = _awgprops_alloc_uninit()
    try:
        fn(slot, dt_ptr)
        # Decode POD fields by direct memory read.
        device_type   = ctypes.c_uint32.from_address(slot + 0x00).value
        max_elf_size  = ctypes.c_uint64.from_address(slot + 0x50).value
        address_impl  = ctypes.c_uint32.from_address(slot + 0x58).value
        sample_format = ctypes.c_uint32.from_address(slot + 0x5c).value
        is_hirzel     = ctypes.c_uint8.from_address(slot + 0x60).value
        # Decode strings via the libc++ shim helpers (work on any
        # pointer to a valid std::string, including interior offsets).
        s_elfdata     = string_bytes(slot + 0x08)
        s_elfprog     = string_bytes(slot + 0x20)
        s_enable      = string_bytes(slot + 0x38)
        s_fpga        = string_bytes(slot + 0x68)
        return (device_type, s_elfdata, s_elfprog, s_enable,
                max_elf_size, address_impl, sample_format, is_hirzel,
                s_fpga)
    finally:
        _awgprops_destroy_in_place(slot)
        _awgprops_free_uninit(slot)


def call_sret_zifolder_2cref(fn: Callable,
                             base_path: bytes,
                             subdir: bytes,
                             extra: bytes) -> bytes:
    """Call `string T::method(string const&, string const&) const`
    where `T` is layout-equivalent to a single std::string at offset 0.
    ABI: void(sret, this*, string*, string*).

    The `this` argument is built by allocating a 24-byte raw slot and
    placement-constructing a libc++ std::string into it (== a ZiFolder
    whose `basePath_` member is `base_path`).  The slot is destroyed
    in place and freed after the call.
    """
    this_slot = alloc_uninit_slot()
    _place_construct(this_slot, base_path, len(base_path))
    sub_p = make_string(subdir)
    ext_p = make_string(extra)
    sret  = alloc_uninit_slot()
    try:
        fn(sret, this_slot, sub_p, ext_p)
        return string_bytes(sret)
    finally:
        destroy_and_free_slot(sret)
        free_string(ext_p)
        free_string(sub_p)
        _destroy_in_place(this_slot)
        _free_uninit(this_slot)


def call_sret_vec_u32_cref(fn: Callable, contents: bytes | None) -> bytes:
    """Call `vector<unsigned int> f(string const&)`.
    ABI: void(sret, string*) where sret is a 24-byte raw slot for a
    libc++ vector<unsigned int> (3-pointer layout).

    The string argument is a filesystem path.  When `contents` is not
    None, this helper materialises a temp file with those bytes and
    passes its path; when None, it passes a deterministic
    non-existent path to exercise the open-fail SHA-1-IV branch.

    Returns the raw digest bytes (size * 4 = typically 20 for SHA-1).
    """
    import os
    import tempfile

    if contents is None:
        # Path that almost certainly does not exist.
        path = b"/tmp/diff_unreachable_nonexistent_" + os.urandom(8).hex().encode()
        cleanup_path = None
    else:
        fd, path_str = tempfile.mkstemp(prefix="diff_unreachable_wave_")
        try:
            os.write(fd, contents)
        finally:
            os.close(fd)
        path = path_str.encode()
        cleanup_path = path_str

    arg = make_string(path)
    sret = _vec_u32_alloc_uninit()
    try:
        fn(sret, arg)
        n_words = _vec_u32_size(sret)
        if n_words == 0:
            return b""
        data_ptr = _vec_u32_data(sret)
        return ctypes.string_at(data_ptr, n_words * 4)
    finally:
        _vec_u32_destroy_in_place(sret)
        _vec_u32_free_uninit(sret)
        free_string(arg)
        if cleanup_path is not None:
            try:
                os.unlink(cleanup_path)
            except OSError:
                pass


def call_sret_str_vec_u32_cref(fn: Callable, words: list[int]) -> bytes:
    """Call `string f(vector<unsigned int> const&)`.
    ABI: void(sret_string, vector*).  Builds the vector argument from
    `words`, calls the sret-string function, returns the result bytes.
    """
    arr = (ctypes.c_uint32 * len(words))(*words) if words else (ctypes.c_uint32 * 0)()
    vec_arg = _vec_u32_make(ctypes.cast(arr, ctypes.c_void_p), len(words))
    sret = alloc_uninit_slot()
    try:
        fn(sret, vec_arg)
        return string_bytes(sret)
    finally:
        destroy_and_free_slot(sret)
        _vec_u32_free(vec_arg)


def call_pod_u16_double_u32(fn: Callable, sample: float, marker: int) -> int:
    """Call `uint16_t f(double, uint32_t)`."""
    return int(fn(sample, marker))


def call_pod_u16_double(fn: Callable, sample: float) -> int:
    """Call `uint16_t f(double)`."""
    return int(fn(sample))


def call_pod_double_u16(fn: Callable, sample: int) -> float:
    """Call `double f(uint16_t)`."""
    v = fn(sample)
    # NaN-aware comparison: bitwise representation comparison via
    # struct.pack rounds NaN bit patterns into a stable form for
    # equality checks.
    return float(v)


def call_pod_u8_u16(fn: Callable, sample: int) -> int:
    """Call `uint8_t f(uint16_t)`."""
    return int(fn(sample)) & 0xff


def call_pod_double_u32(fn: Callable, sample: int) -> float:
    """Call `double f(uint32_t)`."""
    return float(fn(sample))


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
                    "sret_blob", "ref_blob", "cref_ostream_throws"):
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
    if sym.kind == "pod_u32_2u32" or sym.kind == "sret_str_2u32":
        for dval, dlbl in DEVICE_TYPE_CODES:
            for sval, slbl in AWG_SEQUENCER_TYPES:
                yield ((dval, sval), f"({dlbl},{slbl})")
        return
    if sym.kind == "sret_str_u32":
        for sval, slbl in AWG_SEQUENCER_TYPES:
            yield ((sval,), slbl)
        return
    if sym.kind == "sret_blob_cref_throws":
        corpus = THROW_CORPUS_OVERRIDE.get(sym.name, THROW_STRING_INPUTS)
        for data, label in corpus:
            yield ((data,), label)
        return
    if sym.kind == "sret_props_cref":
        corpus = AWGPROPS_DT_CORPUS.get(sym.name) or AWGPROPS_DT_DEFAULT[sym.name]
        for dtype, opts, label in corpus:
            yield ((dtype, opts), label)
        return
    if sym.kind == "sret_zifolder_2cref":
        for base, sub, ext, label in ZIFOLDER_FOLDERPATH_INPUTS:
            yield ((base, sub, ext), label)
        return
    if sym.kind == "sret_vec_u32_cref":
        for contents, label in WAVE_HASH_INPUTS:
            yield ((contents,), label)
        return
    if sym.kind == "sret_str_vec_u32_cref":
        for words, label in WAVE_HASH2STR_INPUTS:
            yield ((words,), label)
        return
    if sym.kind == "pod_u16_double_u32":
        for sample, slbl in DOUBLE2AWG_INPUTS:
            for marker in MARKER_INPUTS:
                yield ((sample, marker), f"{slbl}|m=0x{marker:x}")
        return
    if sym.kind == "pod_u16_double":
        for sample, slbl in DOUBLE2AWG_INPUTS:
            yield ((sample,), slbl)
        return
    if sym.kind in ("pod_double_u16", "pod_u8_u16"):
        for sample, slbl in U16_INPUTS:
            yield ((sample,), slbl)
        return
    if sym.kind == "pod_double_u32":
        for sample, slbl in U32_AWG_INPUTS:
            yield ((sample,), slbl)
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
    elif sym.kind == "sret_blob_cref_throws":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "cref_ostream_throws":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "pod_u32_2u32":
        proto_args = [ctypes.c_uint32, ctypes.c_uint32]
    elif sym.kind == "sret_str_u32":
        proto_args = [ctypes.c_void_p, ctypes.c_uint32]
    elif sym.kind == "sret_str_2u32":
        proto_args = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
    elif sym.kind == "sret_props_cref":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "sret_zifolder_2cref":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p,
                      ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "sret_vec_u32_cref":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "sret_str_vec_u32_cref":
        proto_args = [ctypes.c_void_p, ctypes.c_void_p]
    elif sym.kind == "pod_u16_double_u32":
        proto_args = [ctypes.c_double, ctypes.c_uint32]
    elif sym.kind == "pod_u16_double":
        proto_args = [ctypes.c_double]
    elif sym.kind == "pod_double_u16":
        proto_args = [ctypes.c_uint16]
    elif sym.kind == "pod_u8_u16":
        proto_args = [ctypes.c_uint16]
    elif sym.kind == "pod_double_u32":
        proto_args = [ctypes.c_uint32]
    else:
        raise ValueError(f"unknown kind: {sym.kind}")

    fn_orig = orig_fn(sym.orig_offset, None, proto_args)
    fn_cand = cand_fn(sym.cand_mangled, None, proto_args)
    if sym.kind == "pod_u32_cref":
        # Override the void return with uint32_t for both sides.
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint32, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint32, proto_args)
    elif sym.kind == "pod_u32_2u32":
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
    elif sym.kind == "sret_blob_cref_throws":
        # Same convention as pod_u64_cref2: trampoline takes raw
        # function-pointer addresses.
        fn_orig = ORIG_BASE + sym.orig_offset                 # type: ignore[assignment]
        fn_cand = ctypes.cast(getattr(_cand, sym.cand_mangled),
                              ctypes.c_void_p).value or 0    # type: ignore[assignment]
    elif sym.kind == "cref_ostream_throws":
        # Same convention: trampoline takes raw fn-pointer addresses.
        fn_orig = ORIG_BASE + sym.orig_offset                 # type: ignore[assignment]
        fn_cand = ctypes.cast(getattr(_cand, sym.cand_mangled),
                              ctypes.c_void_p).value or 0    # type: ignore[assignment]
    elif sym.kind in ("pod_u16_double_u32", "pod_u16_double"):
        # uint16_t f(double[, uint32_t])  — return in %ax.
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint16, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint16, proto_args)
    elif sym.kind in ("pod_double_u16", "pod_double_u32"):
        # double f(uint16_t|uint32_t) — return in %xmm0.
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_double, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_double, proto_args)
    elif sym.kind == "pod_u8_u16":
        # uint8_t f(uint16_t) — return in %al.
        fn_orig = orig_fn(sym.orig_offset, ctypes.c_uint8, proto_args)
        fn_cand = cand_fn(sym.cand_mangled, ctypes.c_uint8, proto_args)

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
        elif sym.kind == "sret_blob_cref_throws":
            data7: bytes = args[0]  # type: ignore[assignment]
            blob_size = BLOB_SIZE[sym.name]
            o = call_sret_blob_cref_throws(fn_orig, blob_size, data7)  # type: ignore[arg-type]
            c = call_sret_blob_cref_throws(fn_cand, blob_size, data7)  # type: ignore[arg-type]
            # Both-threw collapse — see pod_u64_cref2 for the full
            # rationale on RTTI-divergence between the statically-linked
            # original and dynamically-linked candidate.
            if o[0] is None and c[0] is None:
                o = (None, "<threw>")
                c = (None, "<threw>")
        elif sym.kind == "cref_ostream_throws":
            blob8: bytes = args[0]  # type: ignore[assignment]
            o = call_cref_ostream_throws(fn_orig, blob8)  # type: ignore[arg-type]
            c = call_cref_ostream_throws(fn_cand, blob8)  # type: ignore[arg-type]
            # Both-threw collapse for the same RTTI reason as above.
            if o[0] is None and c[0] is None:
                o = (None, "<threw>")
                c = (None, "<threw>")
        elif sym.kind == "pod_u32_2u32":
            ai: int = args[0]  # type: ignore[assignment]
            bi: int = args[1]  # type: ignore[assignment]
            o = call_pod_u32_2u32(fn_orig, ai, bi)
            c = call_pod_u32_2u32(fn_cand, ai, bi)
        elif sym.kind == "sret_str_u32":
            v32: int = args[0]  # type: ignore[assignment]
            o = call_sret_str_u32(fn_orig, v32)
            c = call_sret_str_u32(fn_cand, v32)
        elif sym.kind == "sret_str_2u32":
            ai2: int = args[0]  # type: ignore[assignment]
            bi2: int = args[1]  # type: ignore[assignment]
            o = call_sret_str_2u32(fn_orig, ai2, bi2)
            c = call_sret_str_2u32(fn_cand, ai2, bi2)
        elif sym.kind == "sret_props_cref":
            dtype: bytes = args[0]  # type: ignore[assignment]
            opts: bytes  = args[1]  # type: ignore[assignment]
            dt_ptr = make_devicetype(dtype, opts)
            try:
                o = call_sret_props_cref(fn_orig, dt_ptr)
                c = call_sret_props_cref(fn_cand, dt_ptr)
            finally:
                free_devicetype(dt_ptr)
        elif sym.kind == "sret_zifolder_2cref":
            base_z: bytes = args[0]  # type: ignore[assignment]
            sub_z:  bytes = args[1]  # type: ignore[assignment]
            ext_z:  bytes = args[2]  # type: ignore[assignment]
            o = call_sret_zifolder_2cref(fn_orig, base_z, sub_z, ext_z)
            c = call_sret_zifolder_2cref(fn_cand, base_z, sub_z, ext_z)
        elif sym.kind == "sret_vec_u32_cref":
            contents_v: bytes | None = args[0]  # type: ignore[assignment]
            o = call_sret_vec_u32_cref(fn_orig, contents_v)
            c = call_sret_vec_u32_cref(fn_cand, contents_v)
        elif sym.kind == "sret_str_vec_u32_cref":
            words_v: list[int] = args[0]  # type: ignore[assignment]
            o = call_sret_str_vec_u32_cref(fn_orig, words_v)
            c = call_sret_str_vec_u32_cref(fn_cand, words_v)
        elif sym.kind == "pod_u16_double_u32":
            samp_d: float = args[0]  # type: ignore[assignment]
            mark_u: int   = args[1]  # type: ignore[assignment]
            o = call_pod_u16_double_u32(fn_orig, samp_d, mark_u)
            c = call_pod_u16_double_u32(fn_cand, samp_d, mark_u)
        elif sym.kind == "pod_u16_double":
            samp_d2: float = args[0]  # type: ignore[assignment]
            o = call_pod_u16_double(fn_orig, samp_d2)
            c = call_pod_u16_double(fn_cand, samp_d2)
        elif sym.kind == "pod_double_u16":
            samp_u16: int = args[0]  # type: ignore[assignment]
            o = call_pod_double_u16(fn_orig, samp_u16)
            c = call_pod_double_u16(fn_cand, samp_u16)
            # Bit-equal compare for NaN; raw float == otherwise.
            if _math.isnan(o) or _math.isnan(c):
                o = struct.pack('<d', o)  # type: ignore[assignment]
                c = struct.pack('<d', c)  # type: ignore[assignment]
        elif sym.kind == "pod_u8_u16":
            samp_u8: int = args[0]  # type: ignore[assignment]
            o = call_pod_u8_u16(fn_orig, samp_u8)
            c = call_pod_u8_u16(fn_cand, samp_u8)
        elif sym.kind == "pod_double_u32":
            samp_u32: int = args[0]  # type: ignore[assignment]
            o = call_pod_double_u32(fn_orig, samp_u32)
            c = call_pod_double_u32(fn_cand, samp_u32)
            if _math.isnan(o) or _math.isnan(c):
                o = struct.pack('<d', o)  # type: ignore[assignment]
                c = struct.pack('<d', c)  # type: ignore[assignment]
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
