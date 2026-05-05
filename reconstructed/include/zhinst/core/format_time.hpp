// formatTime — time formatting utilities
// Reconstructed from _seqc_compiler.so

#pragma once

#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace zhinst {

// Formats a ptime using an explicit strftime-style format string.
// Internally creates a boost::date_time::time_facet with the given format,
// installs it in a locale on std::cout's locale, then streams through it.
// @0x2f6190, ~0x1b3 bytes
std::string formatTime(boost::posix_time::ptime const& t, char const* fmt);

// Formats a ptime with a predefined format.
// If compact == true:  fmt = "%Y%m%d_%H%M%S"       (at 0x90cf3c)
// If compact == false: fmt = "%Y/%m/%d %H:%M:%S"    (at 0x90cf4a)
// Just dispatches to formatTime(ptime, char const*).
// @0x2f7440, 0x30 bytes
std::string formatTime(boost::posix_time::ptime const& t, bool compact);

// Converts a Unix epoch (seconds) to ptime, then formats.
//   epoch: seconds since Unix epoch
//   compact: selects format string (same as above)
//   utcToLocal: if true, applies boost c_local_adjustor<ptime>::utc_to_local()
//
// Conversion: ptime = epoch(1970-01-01) + microseconds(epoch * 1000000)
//   (the binary multiplies by 0xF4240 = 1000000, then adds the
//    1970-01-01 epoch offset 0x2ed263d83a88000 ticks)
//
// @0x2f7470, 0x78 bytes
std::string formatTime(long epoch, bool compact, bool utcToLocal);

} // namespace zhinst
