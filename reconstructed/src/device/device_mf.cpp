// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_mf.cpp
//
// MF-family device subclasses: Mf, Mfli, Mfia.
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Mfli knownOptions array @ .rodata 0x962458 (9 entries).
constexpr std::array<OptionCodePair<sfc::MfOption>, 9> kMfliKnownOptions = {{
    { sfc::MfOption::MD, DeviceOption::MD   },
    { sfc::MfOption::PID, DeviceOption::PID  },
    { sfc::MfOption::MOD, DeviceOption::MOD  },
    { sfc::MfOption::FF, DeviceOption::FF   },
    { sfc::MfOption::DIG, DeviceOption::DIG  },
    { sfc::MfOption::F5M, DeviceOption::F5M  },
    { sfc::MfOption::BOX, DeviceOption::BOX  },
    { sfc::MfOption::IA, DeviceOption::IA   },  // Mfli-only
    { sfc::MfOption::NOUI, DeviceOption::NOUI },
}};

// Mfia knownOptions array @ .rodata 0x9624a0 (8 entries; no IA bit since
// MFIA is intrinsically IA per isIa()).
constexpr std::array<OptionCodePair<sfc::MfOption>, 8> kMfiaKnownOptions = {{
    { sfc::MfOption::MD, DeviceOption::MD   },
    { sfc::MfOption::PID, DeviceOption::PID  },
    { sfc::MfOption::MOD, DeviceOption::MOD  },
    { sfc::MfOption::FF, DeviceOption::FF   },
    { sfc::MfOption::DIG, DeviceOption::DIG  },
    { sfc::MfOption::F5M, DeviceOption::F5M  },
    { sfc::MfOption::BOX, DeviceOption::BOX  },
    { sfc::MfOption::NOUI, DeviceOption::NOUI },
}};

}  // namespace

// ---------------------------------------------------------------------------
// Mf — empty options
// ---------------------------------------------------------------------------
Mf::Mf() : DeviceTypeImpl(DeviceTypeCode::MF, DeviceFamily::MF) {}
Mf::~Mf() = default;
DeviceTypeImpl* Mf::doClone() const { return new Mf(*this); }

// ---------------------------------------------------------------------------
// Mfli — vtable @ .rodata 0xb09498
// ---------------------------------------------------------------------------
Mfli::Mfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::MFLI, DeviceFamily::MF,
                     initializeSfcOptions(kMfliKnownOptions,
                                          DeviceFamily::MF, opts)) {}
Mfli::~Mfli() = default;
DeviceTypeImpl* Mfli::doClone() const { return new Mfli(*this); }

// ---------------------------------------------------------------------------
// Mfia — vtable @ .rodata 0xb094c0
// ---------------------------------------------------------------------------
Mfia::Mfia(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::MFIA, DeviceFamily::MF,
                     initializeSfcOptions(kMfiaKnownOptions,
                                          DeviceFamily::MF, opts)) {}
Mfia::~Mfia() = default;
DeviceTypeImpl* Mfia::doClone() const { return new Mfia(*this); }

}  // namespace detail
}  // namespace zhinst
