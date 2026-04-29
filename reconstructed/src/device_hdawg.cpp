// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_hdawg.cpp — Phase 14b-ii-b1
//
// HDAWG-family device subclasses: Hdawg, Hdawg4, Hdawg8.
//
// knownOptions arrays verified entry-by-entry against .rodata
// (Hdawg4 @ 0x9626f8, Hdawg8 @ 0x962728) on 2026-04-25.
// ============================================================================

#include "zhinst/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Hdawg4 knownOptions @ .rodata 0x9626f8 (6 entries).
constexpr std::array<OptionCodePair<sfc::HdawgOption>, 6> kHdawg4KnownOptions = {{
    { sfc::HdawgOption::MF, DeviceOption::MF   },
    { sfc::HdawgOption::FF, DeviceOption::FF   },
    { sfc::HdawgOption::ME, DeviceOption::ME   },
    { sfc::HdawgOption::SKW, DeviceOption::SKW  },
    { sfc::HdawgOption::PC, DeviceOption::PC   },
    { sfc::HdawgOption::CNT, DeviceOption::CNT  },
}};

// Hdawg8 knownOptions @ .rodata 0x962728 (6 entries; same shape as Hdawg4).
constexpr std::array<OptionCodePair<sfc::HdawgOption>, 6> kHdawg8KnownOptions = {{
    { sfc::HdawgOption::MF, DeviceOption::MF   },
    { sfc::HdawgOption::FF, DeviceOption::FF   },
    { sfc::HdawgOption::ME, DeviceOption::ME   },
    { sfc::HdawgOption::SKW, DeviceOption::SKW  },
    { sfc::HdawgOption::PC, DeviceOption::PC   },
    { sfc::HdawgOption::CNT, DeviceOption::CNT  },
}};

}  // namespace

Hdawg::Hdawg() : DeviceTypeImpl(DeviceTypeCode::HDAWG, DeviceFamily::HDAWG) {}
Hdawg::~Hdawg() = default;
DeviceTypeImpl* Hdawg::clone() const { return new Hdawg(*this); }

Hdawg4::Hdawg4(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::HDAWG4, DeviceFamily::HDAWG,
                     initializeSfcOptions(kHdawg4KnownOptions,
                                          DeviceFamily::HDAWG, opts)) {}
Hdawg4::~Hdawg4() = default;
DeviceTypeImpl* Hdawg4::clone() const { return new Hdawg4(*this); }

Hdawg8::Hdawg8(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::HDAWG8, DeviceFamily::HDAWG,
                     initializeSfcOptions(kHdawg8KnownOptions,
                                          DeviceFamily::HDAWG, opts)) {}
Hdawg8::~Hdawg8() = default;
DeviceTypeImpl* Hdawg8::clone() const { return new Hdawg8(*this); }

}  // namespace detail
}  // namespace zhinst
