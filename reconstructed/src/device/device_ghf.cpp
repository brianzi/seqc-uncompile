// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_ghf.cpp
//
// GHF-family device subclasses: Ghf, Ghfli.
//
// Note: there is no `sfc::GhfOption`. `Ghfli`'s knownOptions array
// (.rodata 0x96298c) is consumed by `initializeSfcOptions<sfc::ShfOption,5>`
// (mangled symbol observed in the disasm of the Ghfli ctor at 0x2e3a00).
// The 5-entry array matches Shfli's first five entries (the SHF-family
// MF/PID/MOD/FF/AWG bits).
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"
#include "zhinst/device/device_factories.hpp"
#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Ghfli knownOptions @ .rodata 0x96298c (5 entries, sfc::ShfOption typed).
constexpr std::array<OptionCodePair<sfc::ShfOption>, 5> kGhfliKnownOptions = {{
    { sfc::ShfOption::MF, DeviceOption::MF   },
    { sfc::ShfOption::PID, DeviceOption::PID  },
    { sfc::ShfOption::MOD, DeviceOption::MOD  },
    { sfc::ShfOption::FF, DeviceOption::FF   },
    { sfc::ShfOption::AWG, DeviceOption::AWG  },
}};

// Inline FF helper for Ghf (same pattern as Shf/Shfacc).
DeviceOptionSet buildGhfFf(unsigned long opts) {
    DeviceOptionSet set(DeviceFamily::GHF);
    if (opts & kDevFlagFF) set.insert(DeviceOption::FF);
    return set;
}

}  // namespace

// vtable @ .rodata 0xb09b58
Ghf::Ghf(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::GHF, DeviceFamily::GHF,
                     buildGhfFf(opts)) {}
Ghf::~Ghf() = default;
DeviceTypeImpl* Ghf::doClone() const { return new Ghf(*this); }

// vtable @ .rodata 0xb09b80
Ghfli::Ghfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::GHFLI, DeviceFamily::GHF,
                     initializeSfcOptions(kGhfliKnownOptions,
                                          DeviceFamily::GHF, opts)) {}
Ghfli::~Ghfli() = default;
DeviceTypeImpl* Ghfli::doClone() const { return new Ghfli(*this); }

}  // namespace detail
}  // namespace zhinst
