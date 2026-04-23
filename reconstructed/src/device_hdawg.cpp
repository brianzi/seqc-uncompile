// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_hdawg.cpp — Phase 14b-ii-b1
//
// HDAWG-family device subclasses: Hdawg, Hdawg4, Hdawg8.
//
// TODO: 14b-ii-b1 survey recorded that Hdawg4 and Hdawg8 each have a
// 6-entry knownOptions array (.rodata 0x9626f8 and 0x962728). The
// per-entry (mask,code) selection below was inferred from
// sfc::HdawgOption's documented bits but has not been disasm-verified
// entry-by-entry. Re-verify before relying on these for option output.
// ============================================================================

#include "zhinst/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Hdawg4 knownOptions @ .rodata 0x9626f8 (6 entries).
constexpr std::array<OptionCodePair<sfc::HdawgOption>, 6> kHdawg4KnownOptions = {{
    { sfc::HdawgOption::Bit0x00001, DeviceOption::MF   },
    { sfc::HdawgOption::Bit0x00020, DeviceOption::FF   },
    { sfc::HdawgOption::Bit0x00200, DeviceOption::ME   },
    { sfc::HdawgOption::Bit0x00800, DeviceOption::SKW  },
    { sfc::HdawgOption::Bit0x08000, DeviceOption::PC   },
    { sfc::HdawgOption::Bit0x10000, DeviceOption::CNT  },
}};

// Hdawg8 knownOptions @ .rodata 0x962728 (6 entries; same shape as Hdawg4).
constexpr std::array<OptionCodePair<sfc::HdawgOption>, 6> kHdawg8KnownOptions = {{
    { sfc::HdawgOption::Bit0x00001, DeviceOption::MF   },
    { sfc::HdawgOption::Bit0x00020, DeviceOption::FF   },
    { sfc::HdawgOption::Bit0x00200, DeviceOption::ME   },
    { sfc::HdawgOption::Bit0x00800, DeviceOption::SKW  },
    { sfc::HdawgOption::Bit0x08000, DeviceOption::PC   },
    { sfc::HdawgOption::Bit0x10000, DeviceOption::CNT  },
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
