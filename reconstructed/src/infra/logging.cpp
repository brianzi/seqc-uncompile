// Reconstructed implementation for zhinst/logging.hpp.
//
//  — Logging.
//
// Function addresses:
//   - LogRecord::LogRecord(Severity)         @0x2ea220
//   - LogRecord::~LogRecord()                @0x2ea350
//   - LogRecord::operator<<<error_code>(ec)  @0x2afe30
//   - ZiLogger::construct_logger()           @0x2ea660
//
// The bodies below match the boost.log macro expansion observed in the
// binary; nothing here is novel zhinst logic. The original LabOne
// header `/builds/labone/labone/logging/src/zi_logger.hpp` uses the
// BOOST_LOG_GLOBAL_LOGGER family of macros to define ZiLogger plus the
// LOG_TRACE/LOG_DEBUG/etc convenience macros. LogRecord is a
// hand-written record_pump replacement that inlines the stream and
// record into a single object instead of going through
// stream_provider::allocate_compound (which is what stock
// `BOOST_LOG_STREAM_INTERNAL` would do).

#include "zhinst/infra/logging.hpp"

#include <boost/log/core/core.hpp>
#include <boost/log/sources/severity_logger.hpp>

#include <boost/system/error_code.hpp>

#include <exception>
#include <utility>

namespace zhinst::logging {

//! \cond INTERNAL
// The class definitions below are macro-generated (BOOST_LOG_GLOBAL_LOGGER)
// or live in nested namespaces (zhinst::logging::detail) that Doxygen has
// trouble matching against the header declarations. Public documentation
// for ZiLogger and LogRecord lives in zhinst/infra/logging.hpp; suppress
// duplicate processing here to avoid spurious "no matching declaration"
// warnings.

// Implements the `static logger_type construct_logger()` member that
// `BOOST_LOG_GLOBAL_LOGGER(ZiLogger, ...)` declares but does not
// define. Default-constructs the severity logger; the
// /builds/labone/.../zi_logger.hpp original almost certainly uses
// BOOST_LOG_GLOBAL_LOGGER_DEFAULT which expands to the same body.
//
// Reconstruction: 0x2ea660 (records the source location
// "/builds/labone/labone/logging/src/zi_logger.hpp:13" via the
// BOOST_LOG_GLOBAL_LOGGER macro's __FILE__ / __LINE__ capture).
ZiLogger::logger_type ZiLogger::construct_logger() {
    return logger_type();
}

namespace detail {

// 0x2ea220
LogRecord::LogRecord(Severity severity) {
    auto& logger = ZiLogger::get();

    // boost.log's open_record(keywords::severity = level) is exactly
    // what BOOST_LOG_SEV expands to; it internally checks
    // core::get_logging_enabled(), takes the logger's RW mutex,
    // installs the per-thread severity attribute, and returns either
    // an open record or a default-constructed (closed) one.
    record_ = logger.open_record(
        boost::log::keywords::severity = severity);

    // basic_record_ostream<char> is value-initialised by the member
    // initialiser (default ctor: m_record == nullptr).
    if (record_) {
        stream_.attach_record(record_);
        self_ref_ = this;
    }
}

// 0x2ea350
LogRecord::~LogRecord() {
    if (!record_) {
        // Nothing was opened; stream_ was never attached.
        return;
    }

    // Flush the stream so all buffered chars are committed to the
    // record's message attribute.
    stream_.flush();

    // Push the record to the core. Wrap in try/catch so that pipeline
    // errors during sink dispatch don't propagate out of the dtor.
    try {
        ZiLogger::get().push_record(std::move(record_));
    } catch (...) {
        logExceptionToClog(std::current_exception(),
                           "LogRecord dtor",
                           /*include_what=*/true);
    }

    // stream_'s own dtor will detach_from_record().
}

// 0x2afe30 — explicit instantiation for boost::system::error_code.
//
// The binary calls error_code::to_string() and writes the resulting
// std::string into the underlying ostream via
// __put_character_sequence. The default templated body in the header
// is functionally equivalent — boost::system provides
// `operator<<(ostream&, error_code)` that does the same thing — but
// the binary inlines the to_string() path, so we mirror it here for
// byte-for-byte fidelity.
template <>
LogRecord& LogRecord::operator<< <boost::system::error_code>(
    const boost::system::error_code& ec) {
    if (record_) {
        const auto s = ec.to_string();
        stream_.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
    return *this;
}

}  // namespace detail
//! \endcond

}  // namespace zhinst::logging
