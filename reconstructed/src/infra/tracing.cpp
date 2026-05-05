// Reconstructed implementation for zhinst/tracing.hpp.
//
//  — Tracing.
//
// Function addresses:
//   - getDefaultLabOneResource()             @0x0fa3b0
//   - makeDefaultSpanProcessor()             @0x0fa680
//   - TraceProvider::TraceProvider()         @0x0fa560
//   - TraceProvider::~TraceProvider()        @0x0fa5e0
//   - TraceProvider::getInstance()           @0x0fa570
//   - TraceProvider::getProvider()           @0x0fa650
//   - TraceProvider::isEnabled()             @0x0fa670
//   - TraceProvider::enable()                @0x0fa9a0
//   - TraceProvider::disable()               @0x0fa9b0
//   - TraceProvider::forceFlush()            @0x0fa9c0
//   - TraceProvider::shutdown()              @0x0faa50
//   - TraceProvider::configure()             @0x0fa810
//   - ScopedSpan::ScopedSpan(name, scope)    @0x0faea0
//   - ScopedSpan::ScopedSpan(name, scope, attrs) @0x0faae0
//   - ScopedSpan::~ScopedSpan()              @0x0e17c0

#include "zhinst/infra/tracing.hpp"

#include <chrono>
#include <new>
#include <utility>

#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/sdk/common/global_log_handler.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>

namespace zhinst::tracing {

namespace {

// Identical to a hand-rolled `service.commitHash` value embedded in
// the binary at .rodata 0x8fecfd.
constexpr const char kServiceName[]      = "labone";
constexpr const char kServiceNamespace[] = "zhinst";
constexpr const char kServiceVersion[]   = "26.01";
constexpr const char kCommitHash[]       =
    "203353afa6d977a08b0d4178e005ccfb3132992e";

// Default OTLP exporter endpoint (decoded from
// .rodata 0xfa690 — leading '*' is the OtlpHttpExporterOptions schema
// marker; the URL parser strips it so the effective endpoint is
// http://localhost:31318).
constexpr const char kDefaultOtlpEndpoint[] = "*http://localhost:31318";

// File-static BatchSpanProcessorOptions at .rodata 0x956628.
// Default-constructed (no override of the SDK defaults).
const opentelemetry::sdk::trace::BatchSpanProcessorOptions
    kDefaultBatchOptions{};

}  // namespace

// 0x0fa3b0  (415 bytes)
// Binary constructs the Resource in-place as a function-local static,
// zero-inits it, then calls SetAttribute 4 times directly on the
// object's internal AttributeMap.  Each value is a stack-local variant
// with type-tag 5 (const char*).  No intermediate AttributeMap +
// Resource::Create path.
const opentelemetry::sdk::resource::Resource& getDefaultLabOneResource() {
    static opentelemetry::sdk::resource::Resource resource = [] {
        opentelemetry::sdk::resource::Resource r;
        using AV = opentelemetry::common::AttributeValue;
        AV v1{kServiceName};
        r.SetAttribute(opentelemetry::nostd::string_view{"service.name", 12}, v1);
        AV v2{kServiceNamespace};
        r.SetAttribute(opentelemetry::nostd::string_view{"service.namespace", 17}, v2);
        AV v3{kServiceVersion};
        r.SetAttribute(opentelemetry::nostd::string_view{"service.version", 15}, v3);
        AV v4{kCommitHash};
        r.SetAttribute(opentelemetry::nostd::string_view{"service.commitHash", 18}, v4);
        return r;
    }();
    return resource;
}

// 0x0fa680
std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
makeDefaultSpanProcessor() {
    namespace otlp = opentelemetry::exporter::otlp;

    otlp::OtlpHttpExporterOptions opts{};
    opts.url     = kDefaultOtlpEndpoint;
    opts.timeout = otlp::GetOtlpDefaultTracesTimeout();
    opts.http_headers = otlp::GetOtlpDefaultTracesHeaders();

    auto exporter = std::make_unique<otlp::OtlpHttpExporter>(opts);

    return std::make_unique<
        opentelemetry::sdk::trace::BatchSpanProcessor>(
        std::move(exporter), kDefaultBatchOptions);
}

// 0x0fa560 — value-init members; nothing else.
TraceProvider::TraceProvider() : provider_(), enabled_(false) {}

// 0x0fa5e0 — releases the held shared_ptr<TracerProvider>.
TraceProvider::~TraceProvider() = default;

// 0x0fa570
TraceProvider& TraceProvider::getInstance() {
    static TraceProvider provider;
    return provider;
}

// 0x0fa650
opentelemetry::nostd::shared_ptr<opentelemetry::sdk::trace::TracerProvider>
TraceProvider::getProvider() const {
    return provider_;
}

// 0x0fa670
bool TraceProvider::isEnabled() const {
    return enabled_;
}

// 0x0fa9a0
void TraceProvider::enable() {
    enabled_ = true;
}

// 0x0fa9b0
void TraceProvider::disable() {
    enabled_ = false;
}

// 0x0fa9c0
void TraceProvider::forceFlush() {
    if (enabled_ && provider_) {
        provider_->ForceFlush(
            std::chrono::microseconds{std::numeric_limits<int64_t>::max()});
    }
}

// 0x0faa50
void TraceProvider::shutdown() {
    if (enabled_ && provider_) {
        provider_->Shutdown();
    }
}

// 0x0fa810
void TraceProvider::configure(
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor,
    const opentelemetry::sdk::common::AttributeMap& resource_attrs) {
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        resource_attrs, /*schema_url=*/std::string{});

    auto raw = new opentelemetry::sdk::trace::TracerProvider(
        std::move(processor), std::move(resource));
    provider_ = opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::trace::TracerProvider>(raw);

    enabled_ = true;
}

// ─── ScopedSpan ──────────────────────────────────────────────────────

namespace {

// Helper: returns the active TracerProvider raw ptr if the global
// TraceProvider is initialized AND enabled, else nullptr. Both ctors
// guard on this exact condition (see 0xfab0f / 0xfaecc).
inline opentelemetry::sdk::trace::TracerProvider* activeProvider() {
    auto& self = TraceProvider::getInstance();
    if (!self.isEnabled()) {
        return nullptr;
    }
    auto sp = self.getProvider();
    return sp.get();
}

}  // namespace

// 0x0faea0 — no-attributes ctor.
ScopedSpan::ScopedSpan(opentelemetry::nostd::string_view tracer_scope,
                       opentelemetry::nostd::string_view span_name)
    : span_(), scope_engaged_(false) {
    auto* provider = activeProvider();
    if (provider == nullptr) {
        return;  // empty span; dtor short-circuits
    }

    auto tracer = provider->GetTracer(tracer_scope, /*version=*/"",
                                      /*schema_url=*/"");
    span_ = tracer->StartSpan(span_name);

    if (span_) {
        ::new (scope_storage_) opentelemetry::trace::Scope(span_);
        scope_engaged_ = true;
    }
}

// 0x0faae0 — with attributes.
ScopedSpan::ScopedSpan(
    opentelemetry::nostd::string_view tracer_scope,
    opentelemetry::nostd::string_view span_name,
    const std::initializer_list<std::pair<
        opentelemetry::nostd::string_view,
        opentelemetry::common::AttributeValue>>& attributes)
    : span_(), scope_engaged_(false) {
    auto* provider = activeProvider();
    if (provider == nullptr) {
        return;
    }

    auto tracer = provider->GetTracer(tracer_scope, /*version=*/"",
                                      /*schema_url=*/"");
    span_ = tracer->StartSpan(span_name, attributes);

    if (span_) {
        ::new (scope_storage_) opentelemetry::trace::Scope(span_);
        scope_engaged_ = true;
    }
}

// 0x0e17c0
ScopedSpan::~ScopedSpan() {
    if (scope_engaged_) {
        std::launder(reinterpret_cast<opentelemetry::trace::Scope*>(
            scope_storage_))
            ->~Scope();
    }
    if (span_) {
        span_->End();
    }
}

}  // namespace zhinst::tracing
