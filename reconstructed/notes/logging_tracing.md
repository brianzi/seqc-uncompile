# Logging and Tracing {#notes_logging_tracing}

The reconstructed compiler logs through `zhinst::logging` and
emits OpenTelemetry traces through `zhinst::tracing`.

## Logging — `zhinst::logging`

A thin wrapper over **boost.log**.  The single global logger
`ZiLogger` is a `severity_logger_mt<Severity>` declared with
`BOOST_LOG_GLOBAL_LOGGER`.

```cpp
enum class Severity : uint32_t {
    Trace, Debug, Info, Status, Warning, Error, Critical, Fatal
};
```

Public surface:

| Symbol                                | Purpose                                       |
|---------------------------------------|-----------------------------------------------|
| `detail::LogRecord(Severity)`         | RAII record handle; open / stream / push      |
| `detail::LogRecord::operator<<`       | Append a value to the record's stream         |
| `detail::LogRecord::~LogRecord()`     | Flush, push, swallow exceptions to `std::clog`|

Call sites use the standard `BOOST_LOG_SEV(logger, sev) << "…"`
macro pattern; `LogRecord` is the type the macro expands to.

Header / source:

- `reconstructed/include/zhinst/infra/logging.hpp`
- `reconstructed/src/infra/logging.cpp`

## Tracing — `zhinst::tracing`

OpenTelemetry-cpp integration with a kill-switch.  The
`TraceProvider` singleton owns a `nostd::shared_ptr<TracerProvider>`
and a single `enabled` flag — every public method short-circuits
when `enabled == false`, so adding tracing call sites to the
compiler is essentially free at runtime.

Default configuration: OTLP/HTTP exporter pointing at
`http://localhost:31318`, wrapped in a `BatchSpanProcessor` with
default options, decorated with the LabOne service-resource
attributes (`service.name = labone`, `service.namespace = zhinst`,
plus version + commit-hash).

Public surface:

| Symbol                                | Purpose                                       |
|---------------------------------------|-----------------------------------------------|
| `TraceProvider::getInstance()`        | Singleton accessor                            |
| `TraceProvider::configure()`          | (Re-)build the underlying provider; enables   |
| `TraceProvider::enable()` / `disable()` | Toggle the kill-switch                      |
| `TraceProvider::isEnabled()`          | Query the kill-switch                         |
| `TraceProvider::forceFlush()`         | Force-flush the batch span processor          |
| `TraceProvider::shutdown()`           | Shut the underlying provider down             |
| `ScopedSpan(scope, name)`             | RAII span without attributes                  |
| `ScopedSpan(scope, name, attrs)`      | RAII span with `initializer_list` attributes  |

Header / source:

- `reconstructed/include/zhinst/infra/tracing.hpp`
- `reconstructed/src/infra/tracing.cpp`

OpenTelemetry-cpp is not part of the reconstructed build's hard
dependency surface — the `reconstructed/include/opentelemetry/`
tree contains a stub umbrella sufficient to type-check the
tracing call sites.  Production builds must supply the real SDK.

## Verification debt

- **`Severity` enumerator ordering** is inferred from the standard
  boost.log convention and the eight `.rodata` enum-name strings;
  it has not been observed directly at a `LOG_*` call site
  (every existing call is inlined).
- **OpenTelemetry stub fidelity**: the stub headers are sufficient
  for compilation only.  Layouts of `nostd::shared_ptr<T>` and
  `trace::Span` track the libc++ `shared_ptr` layout extended with
  a vtable wrapper for type erasure — they are inferred, not
  byte-verified.
