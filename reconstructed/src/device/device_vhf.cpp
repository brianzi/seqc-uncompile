// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_vhf.cpp
//
// VHF-family device subclasses: Vhf, Vhfli.
//   - Vhf:   inline FF only.
//   - Vhfli: template path with sfc::VhfOption knownOptions @ 0x962aa0 (6).
//
// knownOptions array verified entry-by-entry against .rodata
// (Vhfli @ 0x962aa0) on 2026-04-25.
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"
#include "zhinst/device/device_factories.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Vhfli knownOptions @ .rodata 0x962aa0 (6 entries).
constexpr std::array<OptionCodePair<sfc::VhfOption>, 6> kVhfliKnownOptions = {{
    { sfc::VhfOption::MD, DeviceOption::MD    },
    { sfc::VhfOption::PID, DeviceOption::PID   },
    { sfc::VhfOption::MOD, DeviceOption::MOD   },
    { sfc::VhfOption::FF, DeviceOption::FF    },
    { sfc::VhfOption::AWG, DeviceOption::AWG   },
    { sfc::VhfOption::F200M, DeviceOption::F200M },
}};

DeviceOptionSet buildVhfFf(unsigned long opts) {
    DeviceOptionSet set(DeviceFamily::VHF);
    if (opts & kDevFlagFF) set.insert(DeviceOption::FF);
    return set;
}

}  // namespace

// vtable @ .rodata 0xb09db8
Vhf::Vhf(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::VHF, DeviceFamily::VHF,
                     buildVhfFf(opts)) {}
Vhf::~Vhf() = default;
DeviceTypeImpl* Vhf::doClone() const { return new Vhf(*this); }

// vtable @ .rodata 0xb09de0
Vhfli::Vhfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::VHFLI, DeviceFamily::VHF,
                     initializeSfcOptions(kVhfliKnownOptions,
                                          DeviceFamily::VHF, opts)) {}
Vhfli::~Vhfli() = default;
DeviceTypeImpl* Vhfli::doClone() const { return new Vhfli(*this); }

}  // namespace detail
}  // namespace zhinst
