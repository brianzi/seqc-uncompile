// Reconstructed from _seqc_compiler.so
//
// Sources analyzed:
//   - zhinst::tracing::TraceProvider singleton (provider @0xb84480)
//   - zhinst::tracing::ScopedSpan (RAII span)
//   - zhinst::tracing::getDefaultLabOneResource() @0xfa3b0
//   - zhinst::tracing::makeDefaultSpanProcessor() @0xfa680
//
// TraceProvider layout (singleton storage at .bss 0xb84480..0xb844a0):
//   +0x00  nostd::shared_ptr<TracerProvider>::shared_ptr_wrapper vtable    (8B)
//   +0x08  TracerProvider*                                                 (8B)
//   +0x10  __shared_weak_count*                                            (8B)
//   +0x18  bool enabled                                                    (1B)
//   total: 0x20 (sizeof when accounting for alignment)
//
// ScopedSpan layout (~0x30 bytes):
//   +0x00  nostd::shared_ptr<Span>::shared_ptr_wrapper vtable              (8B)
//   +0x08  Span*                                                           (8B)
//   +0x10  __shared_weak_count*                                            (8B)
//   +0x18  ... (alignment padding / extra weak ref)                        (8B)
//   +0x20  optional<Scope>::value (Scope is sizeof(shared_ptr<Span>)=16)   (16B inline)
//   +0x28  optional<Scope>::has_value bool                                 (1B)
//
// Default Resource attributes (from getDefaultLabOneResource()):
//   "service.name"       = "labone"
//   "service.namespace"  = "zhinst"
//   "service.version"    = "26.01"
//   "service.commitHash" = "203353afa6d977a08b0d4178e005ccfb3132992e"
//
// Default OTLP HTTP exporter endpoint: "*http://localhost:31318"
//   (the leading '*' in the binary literal is an OtlpHttpExporterOptions
//   internal marker — the parser strips it; effective URL is
//   http://localhost:31318)
//
// makeDefaultSpanProcessor builds:
//   BatchSpanProcessor(
//       OtlpHttpExporter(opts{ url=*http://localhost:31318,
//                              timeout=GetOtlpDefaultTracesTimeout(),
//                              headers=GetOtlpDefaultTracesHeaders(),
//                              ... }),
//       BatchSpanProcessorOptions{}  // file-static at 0x956628
//   )

#pragma once

#include <initializer_list>
#include <memory>
#include <utility>

#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/sdk/common/attribute_utils.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>

//! \brief OpenTelemetry-based distributed tracing facade for the
//! SeqC compiler.
//!
//! Hosts a process-wide `TraceProvider` singleton plus the helper
//! `ScopedSpan` RAII type that compiler stages use to bracket their
//! work.  Until `TraceProvider::configure()` is called, every
//! `ScopedSpan` is a no-op; once configured (typically with the OTLP
//! batch processor built by `makeDefaultSpanProcessor()`), spans are
//! published asynchronously via the SDK's batch span processor.
namespace zhinst::tracing {

//! \brief Returns the cached `service.*` `Resource` advertised on
//! every emitted span.
//!
//! \details Function-local static initialised on first call.  The
//! resource carries `service.name=labone`, `service.namespace=zhinst`,
//! `service.version=26.01`, and a build-time commit hash; tracing
//! backends use these attributes to attribute spans to this build.
//! \return Reference to the singleton `Resource`.
const opentelemetry::sdk::resource::Resource& getDefaultLabOneResource();

//! \brief Constructs the default OTLP/HTTP batch span processor.
//!
//! \details Wraps an `OtlpHttpExporter` aimed at
//! `http://localhost:31318` in a `BatchSpanProcessor` configured with
//! the SDK defaults, after sourcing timeout and HTTP headers from
//! the standard `OTEL_EXPORTER_OTLP_*` environment variables.  The
//! returned processor is intended to be handed to
//! `TraceProvider::configure()`.
//! \return Owning pointer to the freshly-constructed processor.
std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
makeDefaultSpanProcessor();

// Process-wide singleton wrapping an OpenTelemetry TracerProvider.
//
// Constructed lazy on first getInstance(). Disabled by default
// (configure() must be called to install a TracerProvider; enable()
// then turns the kill-switch on).
//! Process-wide owner of the OpenTelemetry `TracerProvider` used by
//! every span the compiler emits.
//!
//! The provider is unconfigured at startup: spans created via
//! `ScopedSpan` are no-ops until a host application calls
//! `configure()` to install a span processor (typically the OTLP/HTTP
//! batch processor returned by `makeDefaultSpanProcessor()`). A
//! separate `enable()` / `disable()` kill-switch lets the host pause
//! tracing without tearing down the provider.
//!
//! Resource attributes are merged with a default set
//! (`service.name=labone`, `service.namespace=zhinst`, plus version
//! and commit-hash from `getDefaultLabOneResource()`) so every span
//! carries enough metadata to attribute it to this build.
class TraceProvider {
public:
    // 0xfa570 — function-local-static singleton accessor.
    //! \brief Returns the process-wide `TraceProvider` instance,
    //! constructing it on first call (function-local static).
    //! \return Reference to the singleton instance.
    static TraceProvider& getInstance();

    // 0xfa650 — returns a copy of the inner shared_ptr<TracerProvider>.
    // (Note: passing `&result` as out-parameter rdi in the disassembly,
    // matching libc++ "RVO via hidden pointer" calling convention for
    // 24-byte return types.)
    //! \brief Returns a copy of the held OpenTelemetry
    //! `TracerProvider` shared pointer; the returned pointer is
    //! empty until `configure()` has installed a provider.
    //! \return Shared pointer to the installed provider, or empty
    //! if none has been configured.
    opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::trace::TracerProvider>
    getProvider() const;

    // 0xfa9a0 — enable kill-switch (sets +0x18 = true).
    //! \brief Sets the kill-switch on so subsequent `ScopedSpan`
    //! constructions emit real spans.
    void enable();

    // 0xfa9b0 — disable kill-switch (sets +0x18 = false).
    //! \brief Sets the kill-switch off; subsequent `ScopedSpan`
    //! constructions short-circuit to no-op empty spans.
    void disable();

    // 0xfa670 — read kill-switch.
    //! \brief Returns the current state of the kill-switch
    //! (`true` after `enable()`, `false` after `disable()` or
    //! before the first `configure()` call).
    //! \return `true` if tracing is enabled, `false` otherwise.
    bool isEnabled() const;

    // 0xfa9c0 — if enabled, calls
    //   provider_->ForceFlush(chrono::microseconds::max())
    //! \brief When enabled, calls `provider_->ForceFlush` with an
    //! effectively-infinite timeout so every buffered span is
    //! exported; a no-op when the kill-switch is off.
    void forceFlush();

    // 0xfaa50 — if enabled, calls provider_->Shutdown().
    //! \brief When enabled, calls `provider_->Shutdown()` to flush
    //! and release the underlying exporter; a no-op when the
    //! kill-switch is off.
    void shutdown();

    // 0xfa810 — install a new TracerProvider built from the given
    // span processor and resource attributes (merged with the default
    // labone resource via Resource::Create). Sets enabled = true.
    //! \brief Installs a fresh `TracerProvider` built from
    //! `processor` and the merge of `resource_attrs` with the
    //! default labone resource attributes; sets `enabled_ = true`
    //! as a side effect.
    //! \param processor      Span processor (typically a
    //!                       `BatchSpanProcessor`) the new provider
    //!                       takes ownership of.
    //! \param resource_attrs Caller-supplied resource attributes
    //!                       merged on top of
    //!                       `getDefaultLabOneResource()`.
    void configure(
        std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor,
        const opentelemetry::sdk::common::AttributeMap& resource_attrs);

private:
    // 0xfa560 — zero-init constructor. Friend-accessible only via
    // the singleton.
    //! \brief Private zero-initialising constructor; accessible only
    //! through `getInstance()`.
    TraceProvider();

    // 0xfa5e0 — releases the held shared_ptr<TracerProvider>.
    //! \brief Releases the held provider `shared_ptr`.
    ~TraceProvider();

    //! \brief Copy construction is forbidden — the provider is a
    //! singleton.
    TraceProvider(const TraceProvider&) = delete;
    //! \brief Copy assignment is forbidden — the provider is a
    //! singleton.
    TraceProvider& operator=(const TraceProvider&) = delete;

    // nostd::shared_ptr<TracerProvider> provider_;
    //   +0x00 vtable_wrapper (set during configure())
    //   +0x08 TracerProvider* raw
    //   +0x10 __shared_weak_count* control block
    //! \brief OpenTelemetry tracer provider installed by
    //! `configure()`; empty before the first configuration.
    opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::trace::TracerProvider> provider_;

    // +0x18 bool: false until configure() is called.
    //! \brief Kill-switch flag controlling whether new
    //! `ScopedSpan` instances allocate real spans.
    bool enabled_ = false;
};

// RAII span. Constructor obtains a tracer from the global provider and
// starts a span; destructor ends it (when present). The optional
// `attributes` ctor accepts an initializer_list of key/value pairs
// matching opentelemetry::common::AttributeValue.
//
// Both ctors short-circuit (leaving an empty Span) when the
// TraceProvider singleton is not yet initialized OR is disabled.
//! RAII OpenTelemetry span scoped to the surrounding block.
//!
//! Construct one at the top of a region you want to time or trace,
//! optionally with an initializer list of `(key, AttributeValue)`
//! attributes; the destructor ends the span and pops the active
//! `Scope`. The `tracer_scope` argument selects the named tracer to
//! draw the span from (typically a subsystem name); `span_name`
//! identifies the operation.
//!
//! \binarynote When the global `TraceProvider` is unconfigured or
//! disabled, both constructors short-circuit to an empty span: no
//! tracer lookup, no `StartSpan` call, no attached `Scope`. This
//! makes `ScopedSpan` cheap to leave in hot paths even when tracing
//! is off.
class ScopedSpan {
public:
    // 0xfaea0 — without per-span attributes.
    //! \brief Starts a span named `span_name` on the tracer named
    //! `tracer_scope` and pushes a `Scope` so it becomes the active
    //! span for the surrounding region.
    //! \param tracer_scope Name of the OpenTelemetry tracer to draw
    //!                     the span from (typically a subsystem).
    //! \param span_name    Operation name recorded on the span.
    ScopedSpan(opentelemetry::nostd::string_view tracer_scope,
               opentelemetry::nostd::string_view span_name);

    // 0xfaae0 — with attributes (forwarded to StartSpan).
    //! \brief Same as the two-argument constructor but additionally
    //! forwards `attributes` to `StartSpan` so they are attached to
    //! the new span at creation time.
    //! \param tracer_scope Name of the OpenTelemetry tracer.
    //! \param span_name    Operation name recorded on the span.
    //! \param attributes   `(key, AttributeValue)` pairs attached
    //!                     to the span on creation.
    ScopedSpan(opentelemetry::nostd::string_view tracer_scope,
               opentelemetry::nostd::string_view span_name,
               const std::initializer_list<std::pair<
                   opentelemetry::nostd::string_view,
                   opentelemetry::common::AttributeValue>>& attributes);

    // 0x0e17c0 — End() the span (if any), release Scope (if any).
    //! \brief Ends the span (when one was started) and pops the
    //! associated `Scope`; a no-op for short-circuited empty spans.
    ~ScopedSpan();

    //! \brief Copy construction is forbidden — a span owns
    //! single-use OpenTelemetry state.
    ScopedSpan(const ScopedSpan&) = delete;
    //! \brief Copy assignment is forbidden — a span owns
    //! single-use OpenTelemetry state.
    ScopedSpan& operator=(const ScopedSpan&) = delete;

private:
    // +0x00 .. +0x18  nostd::shared_ptr<Span> span_;
    //! \brief OpenTelemetry span owned by this RAII wrapper; empty
    //! when tracing was disabled at construction time.
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;

    // +0x20 .. +0x28  std::optional<trace::Scope> scope_;
    //   +0x20: trace::Scope value (16B; itself a nostd::shared_ptr<Span>)
    //   +0x28: bool has_value
    //
    // Modeled as a raw aligned storage + bool because trace::Scope is
    // not default-constructible. Constructed in-place via placement-new
    // when the span is non-null.
    //! \brief Aligned storage for the optional `trace::Scope` that
    //! makes `span_` the active span; populated via placement-new
    //! when `span_` is non-empty.
    alignas(opentelemetry::trace::Scope)
        unsigned char scope_storage_[sizeof(opentelemetry::trace::Scope)];
    //! \brief Tracks whether a `Scope` has been constructed in
    //! `scope_storage_`; the destructor destroys the `Scope` only
    //! when this flag is set.
    bool scope_engaged_ = false;
};

}  // namespace zhinst::tracing
