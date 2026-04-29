// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_uhf.cpp — Phase 14b-ii-b1
//
// UHF-family device subclasses: Uhf, Uhfli, Uhfawg, Uhfqa, Uhfia.
//
// Note on knownOptions sizes per survey: Uhfli=10, Uhfawg=7, Uhfqa=5,
// Uhfia=12. All entries verified against .rodata on 2026-04-25.
// Two corrections applied: Uhfawg had {PID,MOD} → {CNT,QA};
// Uhfqa had {MF,CNT} → {FF,RUB}.
// ============================================================================

#include "zhinst/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Uhfli knownOptions @ .rodata 0x962580 (10 entries).
constexpr std::array<OptionCodePair<sfc::UhfOption>, 10> kUhfliKnownOptions = {{
    { sfc::UhfOption::Bit0x00001, DeviceOption::MF   },
    { sfc::UhfOption::Bit0x00002, DeviceOption::PID  },
    { sfc::UhfOption::Bit0x00004, DeviceOption::MOD  },
    { sfc::UhfOption::Bit0x00008, DeviceOption::QA   },
    { sfc::UhfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::UhfOption::Bit0x00200, DeviceOption::AWG  },
    { sfc::UhfOption::Bit0x00400, DeviceOption::DIG  },
    { sfc::UhfOption::Bit0x02000, DeviceOption::RUB  },
    { sfc::UhfOption::Bit0x04000, DeviceOption::BOX  },
    { sfc::UhfOption::Bit0x10000, DeviceOption::CNT  },
}};

// Uhfawg knownOptions @ .rodata 0x9625d0 (7 entries).
// Verified against binary rodata 2026-04-25.
constexpr std::array<OptionCodePair<sfc::UhfOption>, 7> kUhfawgKnownOptions = {{
    { sfc::UhfOption::Bit0x00001, DeviceOption::MF   },
    { sfc::UhfOption::Bit0x00008, DeviceOption::QA   },
    { sfc::UhfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::UhfOption::Bit0x00400, DeviceOption::DIG  },
    { sfc::UhfOption::Bit0x02000, DeviceOption::RUB  },
    { sfc::UhfOption::Bit0x04000, DeviceOption::BOX  },
    { sfc::UhfOption::Bit0x10000, DeviceOption::CNT  },
}};

// Uhfqa knownOptions @ .rodata 0x962608 (5 entries).
// Verified against binary rodata 2026-04-25.
constexpr std::array<OptionCodePair<sfc::UhfOption>, 5> kUhfqaKnownOptions = {{
    { sfc::UhfOption::Bit0x00008, DeviceOption::QA   },
    { sfc::UhfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::UhfOption::Bit0x00400, DeviceOption::DIG  },
    { sfc::UhfOption::Bit0x01000, DeviceOption::QE   },
    { sfc::UhfOption::Bit0x02000, DeviceOption::RUB  },
}};

// Uhfia knownOptions @ .rodata 0x962630 (12 entries).
constexpr std::array<OptionCodePair<sfc::UhfOption>, 12> kUhfiaKnownOptions = {{
    { sfc::UhfOption::Bit0x00001, DeviceOption::MF   },
    { sfc::UhfOption::Bit0x00002, DeviceOption::PID  },
    { sfc::UhfOption::Bit0x00004, DeviceOption::MOD  },
    { sfc::UhfOption::Bit0x00008, DeviceOption::QA   },
    { sfc::UhfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::UhfOption::Bit0x00200, DeviceOption::AWG  },
    { sfc::UhfOption::Bit0x00400, DeviceOption::DIG  },
    { sfc::UhfOption::Bit0x00800, DeviceOption::Option10G },
    { sfc::UhfOption::Bit0x01000, DeviceOption::QE   },
    { sfc::UhfOption::Bit0x02000, DeviceOption::RUB  },
    { sfc::UhfOption::Bit0x04000, DeviceOption::BOX  },
    { sfc::UhfOption::Bit0x10000, DeviceOption::CNT  },
}};

}  // namespace

Uhf::Uhf() : DeviceTypeImpl(DeviceTypeCode::UHF, DeviceFamily::UHF) {}
Uhf::~Uhf() = default;
DeviceTypeImpl* Uhf::clone() const { return new Uhf(*this); }

Uhfli::Uhfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::UHFLI, DeviceFamily::UHF,
                     initializeSfcOptions(kUhfliKnownOptions,
                                          DeviceFamily::UHF, opts)) {}
Uhfli::~Uhfli() = default;
DeviceTypeImpl* Uhfli::clone() const { return new Uhfli(*this); }

Uhfawg::Uhfawg(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::UHFAWG, DeviceFamily::UHF,
                     initializeSfcOptions(kUhfawgKnownOptions,
                                          DeviceFamily::UHF, opts)) {}
Uhfawg::~Uhfawg() = default;
DeviceTypeImpl* Uhfawg::clone() const { return new Uhfawg(*this); }

Uhfqa::Uhfqa(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::UHFQA, DeviceFamily::UHF,
                     initializeSfcOptions(kUhfqaKnownOptions,
                                          DeviceFamily::UHF, opts)) {}
Uhfqa::~Uhfqa() = default;
DeviceTypeImpl* Uhfqa::clone() const { return new Uhfqa(*this); }

Uhfia::Uhfia(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::UHFIA, DeviceFamily::UHF,
                     initializeSfcOptions(kUhfiaKnownOptions,
                                          DeviceFamily::UHF, opts)) {}
Uhfia::~Uhfia() = default;
DeviceTypeImpl* Uhfia::clone() const { return new Uhfia(*this); }

}  // namespace detail
}  // namespace zhinst
