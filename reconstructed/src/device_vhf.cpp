// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_vhf.cpp — Phase 14b-ii-b1
//
// VHF-family device subclasses: Vhf, Vhfli.
//   - Vhf:   inline FF only.
//   - Vhfli: template path with sfc::VhfOption knownOptions @ 0x962aa0 (6).
//
// TODO: per-entry mapping for the Vhfli knownOptions array was inferred
// from sfc::VhfOption documented bits but not disasm-verified.
// ============================================================================

#include "zhinst/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Vhfli knownOptions @ .rodata 0x962aa0 (6 entries).
constexpr std::array<OptionCodePair<sfc::VhfOption>, 6> kVhfliKnownOptions = {{
    { sfc::VhfOption::Bit0x0001, DeviceOption::MD    },
    { sfc::VhfOption::Bit0x0002, DeviceOption::PID   },
    { sfc::VhfOption::Bit0x0004, DeviceOption::MOD   },
    { sfc::VhfOption::Bit0x0020, DeviceOption::FF    },
    { sfc::VhfOption::Bit0x0200, DeviceOption::AWG   },
    { sfc::VhfOption::Bit0x0800, DeviceOption::F200M },
}};

DeviceOptionSet buildVhfFf(unsigned long opts) {
    DeviceOptionSet set(DeviceFamily::VHF);
    if (opts & 0x20ul) set.insert(DeviceOption::FF);
    return set;
}

}  // namespace

// vtable @ .rodata 0xb09db8
Vhf::Vhf(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::VHF, DeviceFamily::VHF,
                     buildVhfFf(opts)) {}
Vhf::~Vhf() = default;
DeviceTypeImpl* Vhf::clone() const { return new Vhf(*this); }

// vtable @ .rodata 0xb09de0
Vhfli::Vhfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::VHFLI, DeviceFamily::VHF,
                     initializeSfcOptions(kVhfliKnownOptions,
                                          DeviceFamily::VHF, opts)) {}
Vhfli::~Vhfli() = default;
DeviceTypeImpl* Vhfli::clone() const { return new Vhfli(*this); }

}  // namespace detail
}  // namespace zhinst
