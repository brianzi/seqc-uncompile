// formatTime — time formatting utilities
// Reconstructed from _seqc_compiler.so

#pragma once

#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace zhinst {

//! \brief Format a `boost::posix_time::ptime` using an
//!        explicit `strftime`-style format string.
//!
//! \details Constructs a
//! `boost::date_time::time_facet<ptime, char>` with `fmt`,
//! installs it on a `std::locale` derived from `std::cout`'s
//! current locale, and streams `t` through it.  Returns the
//! resulting string.
//!
//! \binarynote The intermediate locale is derived from
//!             `std::cout`'s locale, not the global C++
//!             locale, so changes to global locale state
//!             between calls do not affect formatting unless
//!             they have been propagated to `std::cout`.
//!
//! \param t    Time point to format.
//! \param fmt  `strftime`-style format string (NUL-terminated).
//! \return  Formatted time string.
std::string formatTime(boost::posix_time::ptime const& t, char const* fmt);

//! \brief Format a `ptime` with one of two predefined styles.
//!
//! \details Selects the format string by `compact`:
//! `compact == true`  → `"%Y%m%d_%H%M%S"` (filename-safe);
//! `compact == false` → `"%Y/%m/%d %H:%M:%S"` (display).
//! Dispatches to the explicit-format overload.
//!
//! \param t        Time point to format.
//! \param compact  `true` for the underscore-joined compact
//!                 form, `false` for the slash/space display
//!                 form.
//! \return  Formatted time string.
std::string formatTime(boost::posix_time::ptime const& t, bool compact);

//! \brief Format a Unix-epoch second-count using one of the
//!        two predefined styles, with optional UTC →
//!        local-time conversion.
//!
//! \details Converts `epoch` to a `ptime` by adding
//! `microseconds(epoch * 1'000'000)` to the Unix-epoch base
//! (`1970-01-01T00:00:00`).  When `utcToLocal` is `true` the
//! resulting `ptime` is shifted via
//! `boost::date_time::c_local_adjustor<ptime>::utc_to_local`
//! before formatting; otherwise the value is formatted as-is
//! (still nominally UTC).  Format selection follows the same
//! `compact` convention as the `ptime`/`bool` overload.
//!
//! \param epoch       Seconds since the Unix epoch.
//! \param compact     `true` for `"%Y%m%d_%H%M%S"`, `false`
//!                    for `"%Y/%m/%d %H:%M:%S"`.
//! \param utcToLocal  When `true`, convert from UTC to the
//!                    process's local time before formatting.
//! \return  Formatted time string.
std::string formatTime(long epoch, bool compact, bool utcToLocal);

} // namespace zhinst
