# Original Binary Contents — Excluded / Out-of-Scope

Analysis of `_seqc_compiler.so` symbol table comparing the original
binary against the reconstructed `libzhinst_seqc.a`, identifying
everything present in the original that we have deliberately excluded
or not yet reconstructed.

Date: 2026-04-26

## Symbol counts

| Metric | Original `.so` | Reconstructed `.a` |
|--------|---------------:|-------------------:|
| Total defined T/W symbols | 19,625 | 20,531 |
| `zhinst::` symbols | 2,763 | 9,988 |
| `zhinst::` in original only | 1,609 | — |
| `zhinst::` in reconstructed only | — | 8,834 |

The reconstructed library has more `zhinst::` symbols because libstdc++
generates different (and generally more) template instantiations than
libc++, and the static archive retains per-TU symbols that an `.so`
would merge or hide. Of the 1,609 "original-only" symbols, ~1,500 are
libc++ `std::__1::` template instantiations and boost specializations
with no libstdc++ equivalent. **97 symbols are meaningfully absent.**

Of those 97:
- ~40 are **mangling mismatches** — functions we have reconstructed,
  but whose signatures contain `std::string`/`std::vector` which mangle
  differently between libc++ and libstdc++. Functionally present.
- ~27 are **real gaps** being closed in Phase 34.
- ~15 are **SDK utility functions** (out of scope, see below).
- ~10 are **SDK error plumbing** (out of scope, see below).
- ~5 are **libc++ template instantiation differences** (not meaningful).

---

## Statically-linked third-party libraries

The original `.so` statically links several large third-party libraries
that are not part of the compiler's core logic:

### gRPC + Protobuf (~2,700 symbols)

Google's gRPC framework and Protocol Buffers, used exclusively as the
transport layer for OpenTelemetry OTLP trace/metric export. The compiler
instruments its pipeline with distributed tracing spans; in production
these are shipped via gRPC to an OTLP collector.

We stub the OpenTelemetry interface (see below) so this entire stack
is unnecessary.

### OpenTelemetry SDK (~592 symbols)

The full `opentelemetry::` SDK including:
- `TracerProvider`, `Tracer`, `Span`, `SpanContext`
- OTLP HTTP exporter (`OtlpHttpExporter`, `OtlpHttpClient`)
- Metric SDK (`MeterProvider`, `Counter`, `Histogram`)
- Resource and context propagation

Our reconstruction provides stub headers under
`include/opentelemetry/_stub_fwd.hpp` that satisfy the ABI at compile
time. The `zhinst::TraceProvider` and `zhinst::ScopedSpan` wrappers
(14c) are fully reconstructed and delegate to these stubs.

### OpenSSL (hundreds of symbols)

`libssl` and `libcrypto` symbols — TLS support required by the OTLP
HTTP exporter for secure trace export. Not needed without gRPC/OTLP.

### zlib (~50 symbols)

Compression library. Likely used by gRPC/protobuf for message
compression, and possibly by the OTLP exporter. Our build links zlib
dynamically for the ELF writer (ELFIO dependency) but does not need the
gRPC-facing usage.

### ELFIO (~187 symbols)

ELF I/O library for reading and writing ELF binaries (the compiler's
output format). Statically linked in the original; we link dynamically.
Fully utilized — this is part of our reconstruction scope.

### pybind11 (~91 symbols)

Python C++ binding framework. `_seqc_compiler.so` is a CPython extension
module. Our reconstruction includes the full pybind11 binding layer
(`pybind_seqc.cpp`: `pyCompileSeqc`, `makeSeqcCompiler`,
`makeSeqcCompilerInCore`, `PYBIND11_MODULE`). Fully in scope and
complete.

### nlohmann::json (~6 symbols)

JSON library — a few out-of-line symbols. We use `boost::json` for the
same purpose (the original uses both in different places). Not a
meaningful gap.

---

## SDK utility functions (out of scope, ~15 symbols)

General-purpose helper functions that belong to the broader Zurich
Instruments SDK, not to the compiler pipeline. They appear in the binary
because the compiler `.so` is built from a monorepo where these
utilities live in shared translation units or link groups. None are
called by any compiler code path.

| Symbol | Purpose |
|--------|---------|
| `almostEqual(double, double)` | Floating-point approximate comparison |
| `floatEqual(float, float)` | Float overload of above |
| `browseTo(std::string)` | Opens a URL or filesystem path on the host platform |
| `canCreateFileForWriting(path)` | Filesystem write-permission check |
| `entityNameToNumber(std::string)` | XML entity name → numeric codepoint |
| `entityNumberToTxt(std::string)` | XML entity number → text representation |
| `escapeStringForCsharp(std::string)` | C# string literal escaping |
| `escapeStringForJson(std::string&)` | JSON string escaping |
| `escapeStringForMatlab(std::string)` | MATLAB string literal escaping |
| `escapeStringForPython(std::string)` | Python string literal escaping |
| `extractVersionTriple(std::string)` | Parses "major.minor.patch" version strings |
| `initBoostFilesystemForUnicode()` | Platform init for Boost.Filesystem UTF-8 |
| `runningOnMf64Device()` | Runtime check for MF64 device hardware |
| `toSubscript(long)` | Formats a number as Unicode subscript characters |
| `toCheckedString(char const*)` | Null-safe `const char*` → `std::string` |
| `getLaboneVersionWithCommitHash()` | Returns LabOne version string with git hash |

**Rationale for exclusion:** Zero internal call sites from any compiler
code path. These are dead code from the linker's perspective (pulled in
by object-file granularity, not by reference).

---

## SDK error/exception infrastructure (out of scope, ~10 symbols)

The LabOne SDK's error-code and exception hierarchy for the public C
API (`ziAPI`). The compiler uses its own `CompilerMessage`-based error
reporting internally; this layer exists only for the Python-facing API
boundary and is never invoked by compiler logic.

| Symbol | Purpose |
|--------|---------|
| `ErrorKindCategory::message(int)` | `std::error_category` for `ErrorKind` |
| `ErrorKindCategory::name()` | Category name string |
| `ZiApiErrorCategory::default_error_condition(int)` | Maps `ZIResult_enum` → `error_condition` |
| `ZiApiErrorCategory::message(int)` | Human-readable API error message |
| `ZiApiErrorCategory::name()` | Category name string |
| `fromZiErrorKind(ZIErrorKind_enum)` | Converts C enum → C++ `ErrorKind` |
| `toZiErrorKind(ErrorKind)` | Reverse conversion |
| `toApiCode(ErrorKind)` | Maps `ErrorKind` → `ZIResult_enum` |
| `getApiErrorBase/getApiErrorMessage(ZIResult_enum)` | Error message lookup |
| `make_error_condition(ErrorKind)` | `std::error_condition` factory |
| `getKind(Exception const&)` | Extracts `ErrorKind` from exception |
| `special::toApiCode(Exception const&)` | Exception → API code conversion |
| `isApiError(RemoteErrorCode const&)` | Remote error classification |
| `ZIIOException(unsigned long)` | I/O exception constructor |

**Rationale for exclusion:** Zero internal callers from the compiler.
The `ZI*Exception` class hierarchy itself (26 subclasses) IS
reconstructed (Phase 29) because the compiler throws several of them
internally (e.g. `ZIAWGCompilerException`). But the error-code
*plumbing* that maps between `ZIResult_enum`, `ErrorKind`, and
`std::error_condition` is SDK glue with no compiler consumers.

---

## Summary

The original `_seqc_compiler.so` contains roughly 19,600 defined
symbols. Of these:

- **~2,700 are the compiler pipeline** (`zhinst::` namespace) — fully
  reconstructed, with Phase 34 closing the final 27 method/helper gaps.
- **~3,300 are gRPC/Protobuf/OpenSSL** — OTLP trace export transport,
  stubbed out.
- **~600 are OpenTelemetry SDK** — instrumentation framework, stubbed.
- **~200 are ELFIO** — ELF I/O, linked dynamically in our build.
- **~90 are pybind11** — Python binding, fully reconstructed.
- **~25 are SDK utilities + error plumbing** — dead code from monorepo
  linking, deliberately excluded.
- **~12,700 are C++ standard library, Boost, and compiler-generated**
  template instantiations, vtables, typeinfo, guard variables, etc.
