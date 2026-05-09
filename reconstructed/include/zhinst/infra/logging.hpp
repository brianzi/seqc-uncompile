// Reconstructed from _seqc_compiler.so
//
// Sources analyzed:
//   - construct_logger() @0x2ea660 records source location
//     "/builds/labone/labone/logging/src/zi_logger.hpp" line 13.
//     => Original header is /builds/labone/labone/logging/src/zi_logger.hpp
//   - LogRecord ctor @0x2ea220, dtor @0x2ea350.
//   - LogRecord::operator<< <boost::system::error_code> @0x2afe30
//     (only template instantiation exported).
//
// LogRecord layout (libstdc++ / libc++ alignment as observed in binary):
//   +0x000  boost::log::record   record_;     // 8-byte ptr to record_view::public_data
//   +0x008  boost::log::basic_record_ostream<char> stream_;   // ~0x110 bytes
//   +0x110  LogRecord*           self_ref_;   // sentinel: dtor uses to skip if no record
//   total size >= 0x118 bytes.
//
// LogRecord ctor logic (reconstructed):
//   1. Get logger singleton via the BOOST_LOG_GLOBAL_LOGGER tag's static
//      `get()` accessor.
//   2. If logging core is enabled:
//        - lock the logger's RW mutex (severity_logger_mt internals)
//        - set the per-thread severity attribute to `severity`
//        - open a record on the core
//        - unlock
//      Else: record_ remains default-constructed (closed).
//   3. If the record opened, attach the stream to it.
//
// LogRecord dtor logic (reconstructed):
//   1. If record is open:
//        - flush the ostream
//        - core::push_record(record) on the logger
//      With try/catch around the push: any exception →
//          logExceptionToClog(std::current_exception(),
//                             "LogRecord dtor", true).
//
// Implementation note: rather than touch boost.log's internal headers,
// this reconstruction uses the public `BOOST_LOG_GLOBAL_LOGGER` macro
// (which is exactly what the original /builds/labone/.../zi_logger.hpp
// almost certainly used) and the `BOOST_LOG_SEV` machinery indirectly
// via `logger.open_record(keywords::severity = sev)`. The resulting
// behaviour matches the disassembly while keeping the source portable.

#pragma once

#include <boost/log/core/record.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>

#include <exception>

namespace zhinst::logging {

// Severity levels for LabOne logging.
//
// Values are not directly observable from any exported symbol — the only
// consumer in the binary is boost::log's `attribute_value_impl<Severity>`
// which treats it as an opaque uint32. The enumerator names below are
// inferred from string literals present in the binary
// ("Trace", "Debug", "Info", "Status", "Warning", "Error", "Critical",
// "Fatal") and from LabOne's documented logger conventions.
//
// Note: ordering and underlying type inferred from string table order
// ("Trace".."Fatal") and LabOne conventions. Will self-confirm as more
// callers are reconstructed.
enum class Severity : unsigned int {
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Status   = 3,
    Warning  = 4,
    Error    = 5,
    Critical = 6,
    Fatal    = 7,
};

}  // namespace zhinst::logging

// `ZiLogger` is the global-logger tag type used by the LabOne
// /builds/labone/labone/logging/src/zi_logger.hpp header. The
// BOOST_LOG_GLOBAL_LOGGER macro generates the struct (with the same
// name) plus a `static logger_type& get()` accessor, and a
// `static logger_type construct_logger()` initializer that the .cpp
// must define.
//
// We declare it in the namespace it's actually used from
// (`zhinst::logging`) by entering the namespace before the macro and
// exiting after.
namespace zhinst::logging {

BOOST_LOG_GLOBAL_LOGGER(
    ZiLogger,
    boost::log::sources::severity_logger_mt<Severity>)

namespace detail {

// RAII record holder for one log record. Constructed by the LOG_*
// macros at the start of the expression; destruction publishes the
// record to the logging core.
//
// Layout matches what the binary embeds: a `boost::log::record`
// followed by a `basic_record_ostream<char>` and a self-pointer used
// as an "stream is bound to record" flag.
//! RAII helper that opens a Boost.Log record, accumulates streamed
//! values into it, and pushes the finished record to the logging
//! core on destruction.
//!
//! The `LOG_*` macros instantiate one of these for each log
//! statement; user code never constructs it directly. The constructor
//! consults the global filter for `severity` and only opens a record
//! if logging is enabled, so disabled severities cost only a filter
//! check. `operator<<` is a no-op when the record was not opened,
//! making `LOG_TRACE() << expensiveCall()` safe (the call still runs,
//! but its result is discarded cheaply).
//!
//! \binarynote The destructor swallows any exception thrown while
//! pushing the record and routes it through `logExceptionToClog()`
//! rather than allowing it to propagate; logging never throws to
//! its caller.
class LogRecord {
public:
    // 0x2ea220 — opens a record at the given severity if logging is enabled.
    explicit LogRecord(Severity severity);

    // 0x2ea350 — flushes the stream and pushes the record to the core.
    // Catches any exception thrown during push and routes it to
    // zhinst::logging::detail::logExceptionToClog(eptr,
    //     "LogRecord dtor", true).
    ~LogRecord();

    LogRecord(const LogRecord&) = delete;
    LogRecord& operator=(const LogRecord&) = delete;

    // Templated stream insertion. Forwards to the underlying
    // basic_record_ostream<char>. Only the
    // boost::system::error_code instantiation is exported in the
    // binary (@0x2afe30); everything else is header-only and inlined
    // at call sites.
    template <typename T>
    LogRecord& operator<<(const T& value);

private:
    // boost::log::record (8-byte handle).
    boost::log::record record_;

    // boost::log::basic_record_ostream<char> (~0x110 bytes; contains
    // the streambuf, std::ostream, locale, etc.).
    boost::log::basic_record_ostream<char> stream_;

    // Self-pointer used as a "stream is bound to record" flag. Set
    // to `this` after attach_record(); the dtor checks it to know
    // whether the stream needs to be detached.
    LogRecord* self_ref_ = nullptr;
};

// Templated implementation — defined here so callers in any TU can
// stream arbitrary types. The exported error_code instantiation is
// emitted out-of-line in logging.cpp.
template <typename T>
inline LogRecord& LogRecord::operator<<(const T& value) {
    if (record_) {
        stream_ << value;
    }
    return *this;
}

// Forward decl of the helper used by ~LogRecord on exception. Defined
// elsewhere (zhinst::detail logging utilities).
//
// Signature observed at 0x314a30:
//     void logExceptionToClog(std::exception_ptr eptr,
//                             const char* context,
//                             bool include_what);
void logExceptionToClog(std::exception_ptr eptr,
                        const char* context,
                        bool include_what);

}  // namespace detail

}  // namespace zhinst::logging
