# Logging and Tracing

The `zhinst::logging` and `zhinst::tracing` namespaces.
**20 symbols across 17 distinct functions** (8 logging + 12 tracing).
A nm survey originally suggested ~73 symbols; the 53-symbol delta is
boost.log template-instantiation glue and OpenTelemetry/curl
implementation noise unrelated to LabOne code.

Reconstructed in Phase 14c.

## Scope

### `zhinst::logging`

| Symbol | Address | Notes |
|--------|---------|-------|
| `Severity` enum | (no symbol) | Inferred uint32 enum, values 0..7 — names `Trace, Debug, Info, Status, Warning, Error, Critical, Fatal` from .rodata strings + LabOne convention |
| `ZiLogger` tag struct | construct_logger @0x2ea660 | BOOST_LOG_GLOBAL_LOGGER tag; logger type = `severity_logger_mt<Severity>`; source location captured `/builds/labone/labone/logging/src/zi_logger.hpp:13` |
| `logger_singleton<ZiLogger>::init_instance()` | 0x2ea510 | Boost-generated singleton init |
| `lazy_singleton<...>::get()` | 0x2ea430 | Boost-generated singleton accessor |
| `detail::LogRecord::LogRecord(Severity)` | 0x2ea220 | RAII: opens record, attaches stream |
| `detail::LogRecord::~LogRecord()` | 0x2ea350 | flush + push_record + try/catch → logExceptionToClog |
| `detail::LogRecord::operator<< <error_code>` | 0x2afe30 | Only exported template instantiation |
| `detail::logExceptionToClog` | 0x314a30 | Forward-declared; lives in another TU |

### `zhinst::tracing`

| Symbol | Address | Notes |
|--------|---------|-------|
| `getDefaultLabOneResource()` | 0x0fa3b0 | Function-local-static Resource with 4 service attrs |
| `makeDefaultSpanProcessor()` | 0x0fa680 | BatchSpanProcessor(OtlpHttpExporter(opts), kDefaultBatchOptions) |
| `TraceProvider::TraceProvider()` | 0x0fa560 | zero-init |
| `TraceProvider::~TraceProvider()` | 0x0fa5e0 | shared_ptr release |
| `TraceProvider::getInstance()` | 0x0fa570 | function-local-static singleton @ .bss 0xb84480 |
| `TraceProvider::getProvider() const` | 0x0fa650 | returns shared_ptr copy via hidden RVO ptr (24B return type) |
| `TraceProvider::isEnabled() const` | 0x0fa670 | reads bool at +0x18 |
| `TraceProvider::enable()` | 0x0fa9a0 | sets +0x18 = true |
| `TraceProvider::disable()` | 0x0fa9b0 | sets +0x18 = false |
| `TraceProvider::forceFlush()` | 0x0fa9c0 | if enabled: provider->ForceFlush(microseconds::max()) |
| `TraceProvider::shutdown()` | 0x0faa50 | if enabled: provider->Shutdown() |
| `TraceProvider::configure()` | 0x0fa810 | builds new TracerProvider; replaces shared_ptr; enabled=true |
| `ScopedSpan::ScopedSpan(scope, name)` | 0x0faea0 | RAII span without attributes |
| `ScopedSpan::ScopedSpan(scope, name, attrs)` | 0x0faae0 | RAII span with attrs initializer_list |
| `ScopedSpan::~ScopedSpan()` | 0x0e17c0 | destroy Scope (if engaged), End() span (if non-null) |

## Key layouts

### `LogRecord` (≥ 0x118 bytes)

| Offset | Type | Notes |
|--------|------|-------|
| +0x000 | `boost::log::record` | 8-byte handle (ptr to record_view::public_data) |
| +0x008 | `boost::log::basic_record_ostream<char>` | ~0x110 bytes; embedded streambuf, std::ostream sub-object at +0x70 (so std::ostream is at +0x78 from LogRecord base) |
| +0x110 | `LogRecord*` self_ref_ | sentinel: dtor checks to know whether stream is bound |

### `TraceProvider` (0x20 bytes, singleton @ .bss 0xb84480..0xb844a0)

| Offset | Type | Notes |
|--------|------|-------|
| +0x00 | `void*` vtable_wrapper | nostd::shared_ptr<TracerProvider> field 0 |
| +0x08 | `TracerProvider*` | nostd::shared_ptr field 1 (raw ptr) |
| +0x10 | `__shared_weak_count*` | nostd::shared_ptr field 2 (control block) |
| +0x18 | `bool enabled` | kill switch; default false |

### `ScopedSpan` (0x30 bytes)

| Offset | Type | Notes |
|--------|------|-------|
| +0x00 | `void*` vtable_wrapper | nostd::shared_ptr<Span> field 0 |
| +0x08 | `Span*` | nostd::shared_ptr field 1 |
| +0x10 | `__shared_weak_count*` | nostd::shared_ptr field 2 |
| +0x18 | (alignment / padding) | 8 bytes |
| +0x20 | `trace::Scope` | in-place, 16B (itself a nostd::shared_ptr<Span>) |
| +0x28 | `bool` scope_engaged | indicates whether the in-place Scope is constructed |

## Decoded .rodata

### Default service attributes (getDefaultLabOneResource)

| Address | Length | Value |
|---------|--------|-------|
| 0x8ff02c | 12 | `service.name` |
| 0x8ff039 | 6  | `labone` |
| 0x8ff040 | 17 | `service.namespace` |
| 0x8ff052 | 6  | `zhinst` |
| 0x8ff059 | 15 | `service.version` |
| 0x8ff07c | 5  | `26.01` |
| 0x8ff069 | 18 | `service.commitHash` |
| 0x8fecfd | 40 | `203353afa6d977a08b0d4178e005ccfb3132992e` (full git SHA) |

### OTLP exporter

- URL constructed inline at 0x0fa690 from movabs immediates: `*http://localhost:31318` (the leading `*` is an OtlpHttpExporterOptions schema marker; the URL parser strips it, effective endpoint is `http://localhost:31318`).
- BatchSpanProcessorOptions static at .rodata 0x956628 (default-constructed).

### LogRecord dtor

- Magic context string `"LogRecord dtor"` at .rodata 0x90c8ca, passed to `logExceptionToClog`.
- BOOST_LOG_GLOBAL_LOGGER source-location capture string `"/builds/labone/labone/logging/src/zi_logger.hpp"` at .rodata 0x90c8d9, used by `construct_logger()` registration line.

## Reconstruction strategy

- **logging**: implemented against public boost.log headers only. The hand-written `LogRecord` class wraps `boost::log::record` + `basic_record_ostream<char>` (instead of using the stock `record_pump<>` from `<boost/log/sources/record_ostream.hpp>` which allocates a separate `stream_compound`). The `BOOST_LOG_GLOBAL_LOGGER` macro is used to declare the `ZiLogger` tag — this matches what `/builds/labone/.../zi_logger.hpp` almost certainly does.
- **tracing**: OpenTelemetry SDK is not installed on the build host, so `reconstructed/include/opentelemetry/` contains a **stub umbrella** rooted at `_stub_fwd.hpp`. Each public header path the binary's headers would expose (e.g. `opentelemetry/sdk/trace/processor.h`) is a thin shim that includes the umbrella. Sizes/layouts of the stubbed classes (`nostd::shared_ptr`, `trace::Span`, etc.) are documented but not necessarily byte-accurate beyond what the binary requires; the goal is to type-check the reconstructed code, not produce a runnable binary.

## Verification debt

- **Severity enumerator ordering**: not directly observed. Inferred from the 8 enum-name strings `"Trace"…"Fatal"` and from boost.log's standard convention. Definitive verification requires finding any LOG_* macro caller (currently all inlined), which should surface as we reconstruct higher-level functions in later phases.
- **boost.log internal field accesses**: my earlier draft assumed names like `holder->m_Core`, `holder->m_Logger`, `holder->get_mutex()`, `boost::log::aux::exclusive_lock_guard<light_rw_mutex>`. None of those are on the public API surface and I removed all references to them. The current implementation goes through `logger.open_record(keywords::severity = sev)` and `logger.push_record(rec)`, which is what `BOOST_LOG_SEV` ultimately expands to. The behaviour matches the disassembly without poking at private boost.log internals.
- **OpenTelemetry stub fidelity**: the stubs are NOT a substitute for opentelemetry-cpp. Any future linking effort will need real headers. The layout sizes documented above are observed; the specific *member offsets* of `nostd::shared_ptr<T>` (vtable / ptr / ctrl ordering) are inferred from the libc++ `shared_ptr` layout extended with a vtable-wrapper for type erasure, not directly disassembled.
- **`logger_singleton<ZiLogger>::init_instance()` @0x2ea510 and `construct_logger()` @0x2ea660**: these are emitted by the `BOOST_LOG_GLOBAL_LOGGER` / `BOOST_LOG_GLOBAL_LOGGER_DEFAULT` macros respectively; the macro expansion in the reconstructed header re-creates them automatically. The binary's source-location string-literal capture is preserved by virtue of the macro.

## Files added / modified

- `reconstructed/include/zhinst/infra/logging.hpp` — Severity enum, ZiLogger tag (via macro), LogRecord class.
- `reconstructed/src/infra/logging.cpp` — `ZiLogger::construct_logger()`, LogRecord ctor/dtor, error_code instantiation.
- `reconstructed/include/zhinst/infra/tracing.hpp` — TraceProvider singleton, ScopedSpan RAII, free helpers.
- `reconstructed/src/infra/tracing.cpp` — all 11 method bodies + ScopedSpan ctors using `activeProvider()` helper + placement-new Scope storage.
- `reconstructed/include/opentelemetry/_stub_fwd.hpp` — stub umbrella for OpenTelemetry SDK API surface used by tracing.
- `reconstructed/include/opentelemetry/{nostd,common,trace,sdk/{common,resource,trace},exporters/otlp}/*.h` — 14 thin `#include` shim headers.

## Build

`cmake --build .` from `reconstructed/build/` succeeds with no warnings on both new TUs. The static archive `libzhinst_seqc.a` now includes `logging.cpp.o` and `tracing.cpp.o`.
