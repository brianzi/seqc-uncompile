// formatTime — time formatting utilities implementation
// Reconstructed from _seqc_compiler.so

#include "zhinst/core/format_time.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

namespace zhinst {

// ---------------------------------------------------------------------------
// formatTime(ptime const&, char const* fmt) — @0x2f6190, ~0x1b3 bytes
//
// Creates a boost::posix_time::time_facet with the given format string,
// installs it into std::cout's locale, then streams the ptime through
// an ostringstream using that locale.
//
// The disassembly shows:
//   1. Gets locale from std::cout (via ios_base::getloc)
//   2. Allocates a time_facet (0x168 bytes) with `new`
//   3. Constructs time_facet with the format string, default period_formatter,
//      default special_values_formatter, default date_generator_formatter
//   4. Gets the facet's id
//   5. Installs the facet into the locale via locale::__install_ctor
//   6. (streams the ptime — the actual streaming code follows after 0x2f6280)
// ---------------------------------------------------------------------------
std::string formatTime(boost::posix_time::ptime const& t, char const* fmt) {  // @0x2f6190
    using namespace boost::posix_time;
    using facet_t = boost::posix_time::time_facet;

    std::ostringstream oss;
    auto* facet = new facet_t(fmt);
    oss.imbue(std::locale(std::cout.getloc(), facet));
    oss << t;
    return oss.str();
}

// ---------------------------------------------------------------------------
// formatTime(ptime const&, bool compact) — @0x2f7440, 0x30 bytes
//
// Disassembly:
//   lea rcx, [rip+0x615aec]   → 0x90cf3c = "%Y%m%d_%H%M%S"
//   lea rax, [rip+0x615af3]   → 0x90cf4a = "%Y/%m/%d %H:%M:%S"
//   test edx, edx             ; compact
//   cmovne rax, rcx
//   call formatTime(ptime const&, char const*)
// ---------------------------------------------------------------------------
std::string formatTime(boost::posix_time::ptime const& t, bool compact) {  // @0x2f7440
    if (compact)
        return formatTime(t, "%Y%m%d_%H%M%S");
    else
        return formatTime(t, "%Y/%m/%d %H:%M:%S");
}

// ---------------------------------------------------------------------------
// formatTime(long epoch, bool compact, bool utcToLocal) — @0x2f7470, 0x78 bytes
//
// Disassembly:
//   imul rax, rsi, 0xF4240             ; epoch * 1_000_000
//   movabs rdx, 0x2ed263d83a88000      ; boost epoch offset (1400-01-01 → ticks)
//   add  rdx, rax                      ; ptime tick count
//   neg  rax
//   movabs rax, 0x8000000000000000     ; not_a_date_time sentinel
//   cmovno rax, rdx                    ; use calculated value if no overflow
//   test ecx, ecx                      ; utcToLocal
//   je   skip_adjust
//   mov  [rbp-0x18], rax
//   lea  rdi, [rbp-0x18]
//   call c_local_adjustor<ptime>::utc_to_local
// skip_adjust:
//   mov  [rbp-0x20], rax               ; store ptime
//   ; select format string (same as bool overload)
//   test r14b, r14b                    ; compact
//   cmovne rdx, rax                    ; select "%Y%m%d_%H%M%S" or "%Y/%m/%d %H:%M:%S"
//   call formatTime(ptime const&, char const*)
// ---------------------------------------------------------------------------
std::string formatTime(long epoch, bool compact, bool utcToLocal) {  // @0x2f7470
    namespace bpt = boost::posix_time;

    // Convert Unix epoch (seconds) → ptime
    // The binary computes: base_ticks + epoch * 1000000
    // where base_ticks = 0x2ed263d83a88000 (ptime epoch for 1970-01-01 in microseconds)
    bpt::ptime t = bpt::from_time_t(0) + bpt::seconds(epoch);

    if (utcToLocal) {
        using adjuster = boost::date_time::c_local_adjustor<bpt::ptime>;
        t = adjuster::utc_to_local(t);
    }

    return formatTime(t, compact);
}

} // namespace zhinst
