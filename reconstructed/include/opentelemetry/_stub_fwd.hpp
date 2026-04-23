// OpenTelemetry stub: minimal forward decls for reconstructing
// _seqc_compiler.so's zhinst::tracing namespace without depending on
// the actual opentelemetry-cpp installation.
//
// These types model only the API surface that the binary touches.
// Layouts are reconstructed from disassembly observation; sizes match
// what the binary embeds in TraceProvider / ScopedSpan / etc.
//
// **NOT a substitute for the real headers** — every method is
// declared but not defined. This file lets the reconstructed source
// type-check; linking would require either the real opentelemetry-cpp
// or a thin compatibility shim. Phase 14c only commits to a
// reconstructed static library, not to a runnable binary.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

namespace opentelemetry { inline namespace v1 {

namespace nostd {

// `nostd::string_view` in opentelemetry-cpp is a private re-impl of
// std::string_view (so the public API is ABI-stable across C++
// versions). For reconstruction we only need the conversion paths
// that the binary uses.
class string_view {
public:
    constexpr string_view() noexcept = default;
    constexpr string_view(const char* s, std::size_t n) noexcept
        : data_(s), size_(n) {}
    string_view(const char* s) noexcept
        : data_(s), size_(s ? std::char_traits<char>::length(s) : 0) {}
    string_view(const std::string& s) noexcept
        : data_(s.data()), size_(s.size()) {}

    constexpr const char* data() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr std::size_t length() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

private:
    const char* data_ = nullptr;
    std::size_t size_ = 0;
};

// `nostd::shared_ptr<T>` — opentelemetry's private shared_ptr. Layout
// observed in the binary: { void* vtable_wrapper; T* ptr;
// __shared_weak_count* ctrl; } = 0x18 bytes (libc++ shared_ptr layout
// + vtable for type erasure). We model it with three pointers to
// match sizeof; the actual representation doesn't matter for
// reconstruction.
template <typename T>
class shared_ptr {
public:
    shared_ptr() noexcept = default;
    shared_ptr(std::nullptr_t) noexcept {}
    explicit shared_ptr(T* ptr) noexcept : ptr_(ptr) {}

    template <typename U>
    shared_ptr(const shared_ptr<U>& other) noexcept
        : vtable_(other.vtable_), ptr_(other.ptr_), ctrl_(other.ctrl_) {}

    shared_ptr(const shared_ptr&) noexcept = default;
    shared_ptr& operator=(const shared_ptr&) noexcept = default;

    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    template <typename U> friend class shared_ptr;

private:
    void* vtable_ = nullptr;
    T*    ptr_    = nullptr;
    void* ctrl_   = nullptr;
};

}  // namespace nostd

namespace common {

// AttributeValue is a discriminated union over ~15 scalar/array
// types. Only the API surface used by zhinst::tracing matters here
// (it appears as the value-type of the attributes initializer-list
// passed to ScopedSpan and Resource construction).
//
// Real type is `nostd::variant<bool, int32_t, int64_t, ...>`; we
// stub it as an opaque storage blob large enough to hold any
// real-world variant alternative.
struct AttributeValue {
    AttributeValue() = default;
    AttributeValue(const char* v) { (void)v; }
    AttributeValue(nostd::string_view v) { (void)v; }
    AttributeValue(bool v) { (void)v; }
    AttributeValue(int v) { (void)v; }
    AttributeValue(std::int64_t v) { (void)v; }
    AttributeValue(double v) { (void)v; }

private:
    alignas(std::max_align_t) unsigned char storage_[40] = {};
};

}  // namespace common

namespace trace {

// `trace::Span` — abstract base from the OpenTelemetry trace API.
// Only End() is called from zhinst::tracing.
class Span {
public:
    virtual ~Span() = default;
    virtual void End() = 0;
};

// `trace::Tracer` — abstract base. zhinst::tracing only uses
// StartSpan() (two overloads: name-only and name+attributes).
class Tracer {
public:
    virtual ~Tracer() = default;
    virtual nostd::shared_ptr<Span> StartSpan(nostd::string_view name) = 0;
    virtual nostd::shared_ptr<Span> StartSpan(
        nostd::string_view name,
        const std::initializer_list<
            std::pair<nostd::string_view, common::AttributeValue>>&
            attributes) = 0;
};

// `trace::Scope` — RAII helper that pushes a Span as the
// thread-local current span on construct and pops on destruct.
// Layout: a single nostd::shared_ptr<Span> (16 bytes per the binary
// — the vtable_wrapper field is *not* part of Scope, only of
// shared_ptr — so Scope size = 16 here matches alignof(void*)*2).
class Scope {
public:
    explicit Scope(const nostd::shared_ptr<Span>& span) : span_(span) {}
    ~Scope() = default;

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    nostd::shared_ptr<Span> span_;
};

}  // namespace trace

namespace sdk {

namespace common {

// `sdk::common::AttributeMap` is the SDK's owning attribute container
// (vs. opentelemetry::common::KeyValueIterable view). Provides
// SetAttribute(key, value) for both string and other scalar types.
class AttributeMap {
public:
    AttributeMap() = default;

    void SetAttribute(opentelemetry::nostd::string_view key,
                      opentelemetry::common::AttributeValue value) {
        (void)key;
        (void)value;
    }
    void SetAttribute(opentelemetry::nostd::string_view key,
                      const char* value) {
        (void)key;
        (void)value;
    }

private:
    // Real impl holds a flat_map of key -> variant. Reconstruction
    // doesn't need the storage to be functional.
};

}  // namespace common

namespace resource {

// Service-attribute container; produced by the static factory
// `Resource::Create(attrs, schema_url)`.
class Resource {
public:
    Resource() = default;

    static Resource Create(const opentelemetry::sdk::common::AttributeMap& attrs,
                           const std::string& schema_url = std::string{}) {
        (void)attrs;
        (void)schema_url;
        return Resource{};
    }
};

}  // namespace resource

namespace trace {

// Forward-declared base of all span processors (BatchSpanProcessor,
// SimpleSpanProcessor, etc.).
class SpanProcessor {
public:
    virtual ~SpanProcessor() = default;
};

// Real opentelemetry-cpp BatchSpanProcessorOptions has 4 fields
// (max_queue_size, schedule_delay_millis, max_export_batch_size,
// blocking). Stubbed as an empty default-constructed value type.
struct BatchSpanProcessorOptions {
    std::size_t max_queue_size = 2048;
    std::chrono::milliseconds schedule_delay_millis{5000};
    std::size_t max_export_batch_size = 512;
};

// `BatchSpanProcessor` — buffers spans and flushes in batches.
// Reconstruction only constructs one; no methods are called directly.
class BatchSpanProcessor : public SpanProcessor {
public:
    using SpanExporter = SpanProcessor;  // placeholder base
    BatchSpanProcessor(std::unique_ptr<SpanProcessor> exporter,
                       const BatchSpanProcessorOptions& options)
        : exporter_(std::move(exporter)), options_(options) {}

private:
    std::unique_ptr<SpanProcessor> exporter_;
    BatchSpanProcessorOptions options_;
};

// `TracerProvider` — root of the SDK trace pipeline. Reconstructed
// methods used: GetTracer, ForceFlush, Shutdown.
class TracerProvider {
public:
    TracerProvider(std::unique_ptr<SpanProcessor> processor,
                   resource::Resource resource)
        : processor_(std::move(processor)),
          resource_(std::move(resource)) {}
    virtual ~TracerProvider() = default;

    virtual opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
    GetTracer(opentelemetry::nostd::string_view library_name,
              opentelemetry::nostd::string_view library_version = {},
              opentelemetry::nostd::string_view schema_url = {}) {
        (void)library_name;
        (void)library_version;
        (void)schema_url;
        return {};
    }

    virtual bool ForceFlush(std::chrono::microseconds timeout =
                                std::chrono::microseconds::max()) {
        (void)timeout;
        return true;
    }

    virtual bool Shutdown() { return true; }

private:
    std::unique_ptr<SpanProcessor> processor_;
    resource::Resource resource_;
};

}  // namespace trace
}  // namespace sdk

namespace exporter { namespace otlp {

// `OtlpHttpExporterOptions` — controls the OTLP/HTTP exporter.
// Reconstruction only sets `url`, `timeout`, `http_headers`.
struct OtlpHttpExporterOptions {
    std::string url;
    std::chrono::system_clock::duration timeout{std::chrono::seconds{10}};
    // Real type: std::map<std::string, std::string>; we don't care
    // about iteration order here.
    std::vector<std::pair<std::string, std::string>> http_headers;
};

// `OtlpHttpExporter` — POSTs spans as protobuf to a collector.
// Modeled as a SpanProcessor stub for the BatchSpanProcessor wrap.
class OtlpHttpExporter : public opentelemetry::sdk::trace::SpanProcessor {
public:
    explicit OtlpHttpExporter(const OtlpHttpExporterOptions& options)
        : options_(options) {}
private:
    OtlpHttpExporterOptions options_;
};

// Helpers that pull defaults from environment variables. The binary
// calls these to populate timeout/headers when constructing the
// exporter options.
inline std::chrono::system_clock::duration GetOtlpDefaultTracesTimeout() {
    return std::chrono::seconds{10};
}
inline std::vector<std::pair<std::string, std::string>>
GetOtlpDefaultTracesHeaders() {
    return {};
}

}}  // namespace exporter::otlp

}}  // namespace opentelemetry::v1
