// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_shf.cpp — Phase 14b-ii-b1
//
// SHF-family device subclasses: Shf, Shfqa2, Shfqa4, Shfsg2, Shfsg4,
// Shfsg8, Shfqc, Shfli.
//
// Mix of template-driven (Shfqa2, Shfqc, Shfli) and inline (Shf,
// Shfqa4, Shfsg2/4/8) constructors per the 14b-ii-b1 survey.
//
// Inline-bit subclass disassembly mappings:
//   - Shf      : opts & 0x0020  -> insert FF (2)
//   - Shfqa4   : opts & 0x0020  -> insert FF (2)
//                opts & 0x4000  -> insert PLUS (28)
//                opts & 0x8000  -> insert LRT (29)
//   - Shfsg{2,4,8}: opts & 0x0020 -> FF
//                   opts & 0x2000 -> RTR (27)
//                   opts & 0x4000 -> PLUS (28)
//
// TODO: per-entry knownOptions arrays for Shfqa2 (4@0x962850),
// Shfqc (8@0x962870), Shfli (5@0x9628b0) were inferred from the
// sfc::ShfOption documented bits but not disasm-verified.
// ============================================================================

#include "zhinst/device_subclasses.hpp"

#include <array>
#include <utility>

namespace zhinst {
namespace detail {

namespace {

// Shfqa2 knownOptions @ .rodata 0x962850 (4 entries).
constexpr std::array<OptionCodePair<sfc::ShfOption>, 4> kShfqa2KnownOptions = {{
    { sfc::ShfOption::Bit0x00020, DeviceOption::FF    },
    { sfc::ShfOption::Bit0x01000, DeviceOption::Sixteen_W },
    { sfc::ShfOption::Bit0x04000, DeviceOption::PLUS  },
    { sfc::ShfOption::Bit0x08000, DeviceOption::LRT   },
}};

// Shfqc knownOptions @ .rodata 0x962870 (8 entries).
constexpr std::array<OptionCodePair<sfc::ShfOption>, 8> kShfqcKnownOptions = {{
    { sfc::ShfOption::Bit0x00008, DeviceOption::QC2CH },
    { sfc::ShfOption::Bit0x00010, DeviceOption::QC4CH },
    { sfc::ShfOption::Bit0x00020, DeviceOption::FF    },
    { sfc::ShfOption::Bit0x00800, DeviceOption::QC6CH },
    { sfc::ShfOption::Bit0x01000, DeviceOption::Sixteen_W },
    { sfc::ShfOption::Bit0x02000, DeviceOption::RTR   },
    { sfc::ShfOption::Bit0x04000, DeviceOption::PLUS  },
    { sfc::ShfOption::Bit0x08000, DeviceOption::LRT   },
}};

// Shfli knownOptions @ .rodata 0x9628b0 (5 entries).
constexpr std::array<OptionCodePair<sfc::ShfOption>, 5> kShfliKnownOptions = {{
    { sfc::ShfOption::Bit0x00001, DeviceOption::MF   },
    { sfc::ShfOption::Bit0x00002, DeviceOption::PID  },
    { sfc::ShfOption::Bit0x00004, DeviceOption::MOD  },
    { sfc::ShfOption::Bit0x00020, DeviceOption::FF   },
    { sfc::ShfOption::Bit0x00200, DeviceOption::AWG  },
}};

// Inline option-bit helper for the SHF subclasses that don't use the
// template path. Mirrors `if (opts & MASK) set.insert(CODE);` chains.
DeviceOptionSet buildInlineShfOptions(unsigned long opts,
                                      DeviceFamily family,
                                      bool addRtr,    // bit 0x2000 -> RTR (27)
                                      bool addPlus,   // bit 0x4000 -> PLUS (28)
                                      bool addLrt) {  // bit 0x8000 -> LRT (29)
    DeviceOptionSet set(family);
    if (opts & 0x0020ul) set.insert(DeviceOption::FF);
    if (addRtr  && (opts & 0x2000ul)) set.insert(DeviceOption::RTR);
    if (addPlus && (opts & 0x4000ul)) set.insert(DeviceOption::PLUS);
    if (addLrt  && (opts & 0x8000ul)) set.insert(DeviceOption::LRT);
    return set;
}

}  // namespace

// ---------------------------------------------------------------------------
// Shf — inline FF only. vtable @ .rodata 0xb09808.
// ---------------------------------------------------------------------------
Shf::Shf(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHF, DeviceFamily::SHF,
                     buildInlineShfOptions(opts, DeviceFamily::SHF,
                                           false, false, false)) {}
Shf::~Shf() = default;
DeviceTypeImpl* Shf::clone() const { return new Shf(*this); }

// ---------------------------------------------------------------------------
// Shfqa2 — template path, vtable @ .rodata 0xb09830.
// ---------------------------------------------------------------------------
Shfqa2::Shfqa2(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFQA2, DeviceFamily::SHF,
                     initializeSfcOptions(kShfqa2KnownOptions,
                                          DeviceFamily::SHF, opts)) {}
Shfqa2::~Shfqa2() = default;
DeviceTypeImpl* Shfqa2::clone() const { return new Shfqa2(*this); }

// ---------------------------------------------------------------------------
// Shfqa4 — inline FF + PLUS + LRT. vtable @ .rodata 0xb09858.
// ---------------------------------------------------------------------------
Shfqa4::Shfqa4(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFQA4, DeviceFamily::SHF,
                     buildInlineShfOptions(opts, DeviceFamily::SHF,
                                           false, true, true)) {}
Shfqa4::~Shfqa4() = default;
DeviceTypeImpl* Shfqa4::clone() const { return new Shfqa4(*this); }

// ---------------------------------------------------------------------------
// Shfsg2 / Shfsg4 / Shfsg8 — inline FF + RTR + PLUS, all identical.
// ---------------------------------------------------------------------------
Shfsg2::Shfsg2(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFSG2, DeviceFamily::SHF,
                     buildInlineShfOptions(opts, DeviceFamily::SHF,
                                           true, true, false)) {}
Shfsg2::~Shfsg2() = default;
DeviceTypeImpl* Shfsg2::clone() const { return new Shfsg2(*this); }

Shfsg4::Shfsg4(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFSG4, DeviceFamily::SHF,
                     buildInlineShfOptions(opts, DeviceFamily::SHF,
                                           true, true, false)) {}
Shfsg4::~Shfsg4() = default;
DeviceTypeImpl* Shfsg4::clone() const { return new Shfsg4(*this); }

Shfsg8::Shfsg8(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFSG8, DeviceFamily::SHF,
                     buildInlineShfOptions(opts, DeviceFamily::SHF,
                                           true, true, false)) {}
Shfsg8::~Shfsg8() = default;
DeviceTypeImpl* Shfsg8::clone() const { return new Shfsg8(*this); }

// ---------------------------------------------------------------------------
// Shfqc — template path, vtable @ .rodata 0xb098f8.
// ---------------------------------------------------------------------------
Shfqc::Shfqc(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFQC, DeviceFamily::SHF,
                     initializeSfcOptions(kShfqcKnownOptions,
                                          DeviceFamily::SHF, opts)) {}
Shfqc::~Shfqc() = default;
DeviceTypeImpl* Shfqc::clone() const { return new Shfqc(*this); }

// ---------------------------------------------------------------------------
// Shfli — template path, vtable @ .rodata 0xb09920.
// ---------------------------------------------------------------------------
Shfli::Shfli(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFLI, DeviceFamily::SHF,
                     initializeSfcOptions(kShfliKnownOptions,
                                          DeviceFamily::SHF, opts)) {}
Shfli::~Shfli() = default;
DeviceTypeImpl* Shfli::clone() const { return new Shfli(*this); }

}  // namespace detail
}  // namespace zhinst
