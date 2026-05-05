// Reconstructed from _seqc_compiler.so — Phase 14c (Tracing).
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

namespace zhinst::tracing {

// Returns the cached function-local-static Resource describing this
// service: name=labone, namespace=zhinst, version=26.01,
// commitHash=<hex>.
//
// Reconstruction: 0xfa3b0. Magic-init-once guarded.
const opentelemetry::sdk::resource::Resource& getDefaultLabOneResource();

// Builds the default span processor: a BatchSpanProcessor wrapping
// an OtlpHttpExporter pointed at http://localhost:31318.
//
// Reconstruction: 0xfa680. Pulls timeout/headers from the env via
// opentelemetry::v1::exporter::otlp::GetOtlpDefaultTraces{Timeout,Headers}.
std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
makeDefaultSpanProcessor();

// Process-wide singleton wrapping an OpenTelemetry TracerProvider.
//
// Constructed lazy on first getInstance(). Disabled by default
// (configure() must be called to install a TracerProvider; enable()
// then turns the kill-switch on).
class TraceProvider {
public:
    // 0xfa570 — function-local-static singleton accessor.
    static TraceProvider& getInstance();

    // 0xfa650 — returns a copy of the inner shared_ptr<TracerProvider>.
    // (Note: passing `&result` as out-parameter rdi in the disassembly,
    // matching libc++ "RVO via hidden pointer" calling convention for
    // 24-byte return types.)
    opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::trace::TracerProvider>
    getProvider() const;

    // 0xfa9a0 — enable kill-switch (sets +0x18 = true).
    void enable();

    // 0xfa9b0 — disable kill-switch (sets +0x18 = false).
    void disable();

    // 0xfa670 — read kill-switch.
    bool isEnabled() const;

    // 0xfa9c0 — if enabled, calls
    //   provider_->ForceFlush(chrono::microseconds::max())
    void forceFlush();

    // 0xfaa50 — if enabled, calls provider_->Shutdown().
    void shutdown();

    // 0xfa810 — install a new TracerProvider built from the given
    // span processor and resource attributes (merged with the default
    // labone resource via Resource::Create). Sets enabled = true.
    void configure(
        std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor,
        const opentelemetry::sdk::common::AttributeMap& resource_attrs);

private:
    // 0xfa560 — zero-init constructor. Friend-accessible only via
    // the singleton.
    TraceProvider();

    // 0xfa5e0 — releases the held shared_ptr<TracerProvider>.
    ~TraceProvider();

    TraceProvider(const TraceProvider&) = delete;
    TraceProvider& operator=(const TraceProvider&) = delete;

    // nostd::shared_ptr<TracerProvider> provider_;
    //   +0x00 vtable_wrapper (set during configure())
    //   +0x08 TracerProvider* raw
    //   +0x10 __shared_weak_count* control block
    opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::trace::TracerProvider> provider_;

    // +0x18 bool: false until configure() is called.
    bool enabled_ = false;
};

// RAII span. Constructor obtains a tracer from the global provider and
// starts a span; destructor ends it (when present). The optional
// `attributes` ctor accepts an initializer_list of key/value pairs
// matching opentelemetry::common::AttributeValue.
//
// Both ctors short-circuit (leaving an empty Span) when the
// TraceProvider singleton is not yet initialized OR is disabled.
class ScopedSpan {
public:
    // 0xfaea0 — without per-span attributes.
    ScopedSpan(opentelemetry::nostd::string_view tracer_scope,
               opentelemetry::nostd::string_view span_name);

    // 0xfaae0 — with attributes (forwarded to StartSpan).
    ScopedSpan(opentelemetry::nostd::string_view tracer_scope,
               opentelemetry::nostd::string_view span_name,
               const std::initializer_list<std::pair<
                   opentelemetry::nostd::string_view,
                   opentelemetry::common::AttributeValue>>& attributes);

    // 0x0e17c0 — End() the span (if any), release Scope (if any).
    ~ScopedSpan();

    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;

private:
    // +0x00 .. +0x18  nostd::shared_ptr<Span> span_;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;

    // +0x20 .. +0x28  std::optional<trace::Scope> scope_;
    //   +0x20: trace::Scope value (16B; itself a nostd::shared_ptr<Span>)
    //   +0x28: bool has_value
    //
    // Modeled as a raw aligned storage + bool because trace::Scope is
    // not default-constructible. Constructed in-place via placement-new
    // when the span is non-null.
    alignas(opentelemetry::trace::Scope)
        unsigned char scope_storage_[sizeof(opentelemetry::trace::Scope)];
    bool scope_engaged_ = false;
};

}  // namespace zhinst::tracing
