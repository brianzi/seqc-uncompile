// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_mf.cpp — Phase 14b-ii-b1
//
// MF-family device subclasses: Mf, Mfli, Mfia.
// ============================================================================

#include "zhinst/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Mfli knownOptions array @ .rodata 0x962458 (9 entries).
constexpr std::array<OptionCodePair<sfc::MfOption>, 9> kMfliKnownOptions = {{
    { sfc::MfOption::Bit0x00001, DeviceOption::MD   },
    { sfc::MfOption::Bit0x00002, DeviceOption::PID  },
    { sfc::MfOption::Bit0x00004, DeviceOption::MOD  },
    { sfc::MfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::MfOption::Bit0x00400, DeviceOption::DIG  },
    { sfc::MfOption::Bit0x00800, DeviceOption::F5M  },
    { sfc::MfOption::Bit0x04000, DeviceOption::BOX  },
    { sfc::MfOption::Bit0x08000, DeviceOption::IA   },  // Mfli-only
    { sfc::MfOption::Bit0x20000, DeviceOption::NOUI },
}};

// Mfia knownOptions array @ .rodata 0x9624a0 (8 entries; no IA bit since
// MFIA is intrinsically IA per isIa()).
constexpr std::array<OptionCodePair<sfc::MfOption>, 8> kMfiaKnownOptions = {{
    { sfc::MfOption::Bit0x00001, DeviceOption::MD   },
    { sfc::MfOption::Bit0x00002, DeviceOption::PID  },
    { sfc::MfOption::Bit0x00004, DeviceOption::MOD  },
    { sfc::MfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::MfOption::Bit0x00400, DeviceOption::DIG  },
    { sfc::MfOption::Bit0x00800, DeviceOption::F5M  },
    { sfc::MfOption::Bit0x04000, DeviceOption::BOX  },
    { sfc::MfOption::Bit0x20000, DeviceOption::NOUI },
}};

}  // namespace

// ---------------------------------------------------------------------------
// Mf — empty options
// ---------------------------------------------------------------------------
Mf::Mf() : DeviceTypeImpl(DeviceTypeCode::MF, DeviceFamily::MF) {}
Mf::~Mf() = default;
DeviceTypeImpl* Mf::clone() const { return new Mf(*this); }

// ---------------------------------------------------------------------------
// Mfli — vtable @ .rodata 0xb09498
// ---------------------------------------------------------------------------
Mfli::Mfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::MFLI, DeviceFamily::MF,
                     initializeSfcOptions(kMfliKnownOptions,
                                          DeviceFamily::MF, opts)) {}
Mfli::~Mfli() = default;
DeviceTypeImpl* Mfli::clone() const { return new Mfli(*this); }

// ---------------------------------------------------------------------------
// Mfia — vtable @ .rodata 0xb094c0
// ---------------------------------------------------------------------------
Mfia::Mfia(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::MFIA, DeviceFamily::MF,
                     initializeSfcOptions(kMfiaKnownOptions,
                                          DeviceFamily::MF, opts)) {}
Mfia::~Mfia() = default;
DeviceTypeImpl* Mfia::clone() const { return new Mfia(*this); }

}  // namespace detail
}  // namespace zhinst
