// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
//
// generateMfSfc — encode MFLI/MFIA Smart Feature Code (SFC) bitfield.
//
// Source path (from debug info): /builds/labone/labone/device/types/src/
//                                generate_mf_sfc.cpp
// Binary address: 0x2de910 .. 0x2dead0
// ============================================================================

#include "zhinst/device/device_type.hpp"
#include "zhinst/core/exception.hpp"

#include <boost/throw_exception.hpp>

#include <cstdint>
#include <sstream>
#include <string>

namespace zhinst {
namespace detail {

namespace {

// Helper: shift a bool to a specific bit position.
constexpr uint64_t bitIf(bool set, unsigned shift) {
    return set ? (uint64_t{1} << shift) : uint64_t{0};
}

}  // namespace

// ============================================================================
// generateMfSfc — @ 0x2de910
//
// Bit layout (verified against disassembly; both MFLI and MFIA use the
// same option-to-bit mapping, differing only in their "always-on" base
// bit):
//
//   bit  0  : DeviceOption::MD     (option index 1)
//   bit  1  : DeviceOption::PID    (option index 4)
//   bit  2  : DeviceOption::MOD    (option index 5)
//   bit  5  : DeviceOption::FF     (option index 2)
//   bit  6  : 1 if MFLI            (always-on for type code 10)
//   bit  7  : 1 if MFIA            (always-on for type code 11)
//   bit 10  : DeviceOption::DIG    (option index 9)
//   bit 11  : DeviceOption::F5M    (option index 12)
//   bit 14  : DeviceOption::BOX    (option index 14)
//   bit 15  : DeviceOption::IA     (option index 15)
//   bit 17  : DeviceOption::NOUI   (option index 18)
//
// Throws zhinst::Exception if the device-type-name string doesn't map
// to MFLI (10) or MFIA (11). The exception is thrown via
// boost::throw_exception with source_location { file=
// "/builds/labone/labone/device/types/src/generate_mf_sfc.cpp",
// function="sfc::FeaturesCode zhinst::detail::generateMfSfc(...)",
// line=104, column=47 }.
//
// return type promoted from the `uint64_t` stand-in to the
// real `sfc::FeaturesCode` strong-typed wrapper. Layout is identical
// (single trailing uint64), so the previous bitwise composition and
// the binary's `rax`-return convention both still hold byte-for-byte.
// ============================================================================
sfc::FeaturesCode generateMfSfc(std::string const& deviceTypeName,
                                DeviceOptionSet const& options) {
    auto const code = toDeviceTypeCode(deviceTypeName);

    // Common option-bit composition shared by MFLI and MFIA.
    auto const commonBits =
          bitIf(options.contains(DeviceOption::MD),    0)
        | bitIf(options.contains(DeviceOption::PID),   1)
        | bitIf(options.contains(DeviceOption::MOD),   2)
        | bitIf(options.contains(DeviceOption::FF),    5)
        | bitIf(options.contains(DeviceOption::DIG),  10)
        | bitIf(options.contains(DeviceOption::F5M),  11)
        | bitIf(options.contains(DeviceOption::BOX),  14)
        | bitIf(options.contains(DeviceOption::NOUI), 17)
        | bitIf(options.contains(DeviceOption::IA),   15);

    if (code == DeviceTypeCode::MFIA) {
        return sfc::FeaturesCode{commonBits | uint64_t{0x80}};   // bit 7
    }
    if (code == DeviceTypeCode::MFLI) {
        return sfc::FeaturesCode{commonBits | uint64_t{0x40}};   // bit 6
    }

    // Unsupported device type — format & throw.
    std::ostringstream oss;
    oss << "Requested to generate an SFC for an unsupported device type ("
        << code  // operator<<(ostream&, DeviceTypeCode) @ 0x2d4350
        << ").";
    BOOST_THROW_EXCEPTION(Exception(oss.str()));
}

}  // namespace detail
}  // namespace zhinst
