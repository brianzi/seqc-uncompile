// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_hf2.cpp
//
// HF2-family device subclasses: Hf2, Hf2li, Hf2is.
//
// All three derive from detail::DeviceTypeImpl. Inheritance only
// substitutes the vptr; no extra data members.
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Hf2li knownOptions array @ .rodata 0x962394 (7 entries).
constexpr std::array<OptionCodePair<sfc::Hf2Option>, 7> kHf2liKnownOptions = {{
    { sfc::Hf2Option::MF, DeviceOption::MF   },  // -> "MFK" for HF2 family
    { sfc::Hf2Option::PLL, DeviceOption::PLL  },
    { sfc::Hf2Option::MOD, DeviceOption::MOD  },
    { sfc::Hf2Option::RT, DeviceOption::RT   },  // -> "RTK" for HF2
    { sfc::Hf2Option::UHS, DeviceOption::UHS  },
    { sfc::Hf2Option::PID, DeviceOption::PID  },
    { sfc::Hf2Option::WEB, DeviceOption::WEB  },
}};

// Hf2is knownOptions array @ .rodata 0x9623cc (6 entries).
constexpr std::array<OptionCodePair<sfc::Hf2Option>, 6> kHf2isKnownOptions = {{
    { sfc::Hf2Option::MF, DeviceOption::MF   },
    { sfc::Hf2Option::PLL, DeviceOption::PLL  },
    { sfc::Hf2Option::MOD, DeviceOption::MOD  },
    { sfc::Hf2Option::RT, DeviceOption::RT   },
    { sfc::Hf2Option::UHS, DeviceOption::UHS  },
    { sfc::Hf2Option::PID, DeviceOption::PID  },
}};

}  // namespace

// ---------------------------------------------------------------------------
// Hf2 — @ 0x2e0830, vtable @ .rodata 0xb09368
// Empty options (just code+family).
// ---------------------------------------------------------------------------
Hf2::Hf2()
    : DeviceTypeImpl(DeviceTypeCode::HF2, DeviceFamily::HF2) {}

Hf2::~Hf2() = default;

DeviceTypeImpl* Hf2::clone() const { return new Hf2(*this); }

// ---------------------------------------------------------------------------
// Hf2li — vtable @ .rodata 0xb09390
// Calls initializeSfcOptions<Hf2Option,7> with kHf2liKnownOptions.
// ---------------------------------------------------------------------------
Hf2li::Hf2li(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::HF2LI, DeviceFamily::HF2,
                     initializeSfcOptions(kHf2liKnownOptions,
                                          DeviceFamily::HF2, opts)) {}

Hf2li::~Hf2li() = default;

DeviceTypeImpl* Hf2li::clone() const { return new Hf2li(*this); }

// ---------------------------------------------------------------------------
// Hf2is — vtable @ .rodata 0xb093b8
// Calls initializeSfcOptions<Hf2Option,6> with kHf2isKnownOptions.
// ---------------------------------------------------------------------------
Hf2is::Hf2is(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::HF2IS, DeviceFamily::HF2,
                     initializeSfcOptions(kHf2isKnownOptions,
                                          DeviceFamily::HF2, opts)) {}

Hf2is::~Hf2is() = default;

DeviceTypeImpl* Hf2is::clone() const { return new Hf2is(*this); }

}  // namespace detail
}  // namespace zhinst
