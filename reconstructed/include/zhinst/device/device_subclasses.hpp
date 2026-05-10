// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_subclasses.hpp
//
// Per-family `detail::*` device subclasses derived from
// `detail::DeviceTypeImpl`. All 32 subclasses share the same layout as
// the base (88 bytes / 0x58); they add no data members and only
// override the vptr at +0x00. Each provides:
//   - one constructor (default, or `unsigned long opts`)
//   - virtual destructor   (vtable[1])
//   - `clone()` override   (vtable[0])
//
// Constructor patterns (decoded in 14b-ii-b1):
//   - `Xxx()` zero-arg ctors: just call `DeviceTypeImpl(code, family)`
//   - `Xxx(unsigned long opts)` ctors: build a `DeviceOptionSet` from
//     `opts` either via `detail::initializeSfcOptions<T,N>(knownOptions,
//     family, opts)` or via an inline if/insert chain on a few selector
//     bits, then call `DeviceTypeImpl(code, family, std::move(set))`.
//
// `knownOptions` arrays for the 15 template-using subclasses are file-
// local in each family's .cpp (anonymous-namespace constexpr arrays).
//
// Factory classes (Hf2Factory, MfFactory, ..., VhfFactory) +
// `detail::makeDeviceFamilyFactory` + `detail::UnknownDevice` +
// `detail::GenericDeviceType` + `DeviceType(string, vector<string>)`
// parser are deferred to 
//
// See reconstructed/notes/device_type.md for the full
// subclass survey table (codes, families, vtables, init patterns).
// ============================================================================
#pragma once

#include "zhinst/device/device_type.hpp"

namespace zhinst {
namespace detail {

// ---------------------------------------------------------------------------
// HF2 family (DeviceFamily::HF2 = 0x001)
// ---------------------------------------------------------------------------
//! \brief HF2 family marker (zero-arg, no options).
class Hf2 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HF2` and family `HF2`; option set is empty.
    Hf2();                                          // @ 0x2e0830, vtable b09368
    //! \brief Trivial defaulted destructor.
    ~Hf2() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hf2`.
    //! \return Newly-allocated `Hf2` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief HF2LI lock-in amplifier; decodes options via
//! `initializeSfcOptions` against `sfc::Hf2Option`.
class Hf2li : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HF2LI` and family `HF2`, mapping
    //!        `opts` through the HF2LI `knownOptions` table.
    //! \param opts Raw HF2-family options bitmask.
    explicit Hf2li(unsigned long opts);             // vtable b09390
    //! \brief Trivial defaulted destructor.
    ~Hf2li() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hf2li`.
    //! \return Newly-allocated `Hf2li` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief HF2IS impedance spectroscope; decodes options via
//! `initializeSfcOptions` against `sfc::Hf2Option`.
class Hf2is : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HF2IS` and family `HF2`, mapping
    //!        `opts` through the HF2IS `knownOptions` table.
    //! \param opts Raw HF2-family options bitmask.
    explicit Hf2is(unsigned long opts);             // vtable b093b8
    //! \brief Trivial defaulted destructor.
    ~Hf2is() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hf2is`.
    //! \return Newly-allocated `Hf2is` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// MF family (DeviceFamily::MF = 0x004)
// ---------------------------------------------------------------------------
//! \brief MF family marker (zero-arg, no options).
class Mf : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `MF` and family `MF`; option set is empty.
    Mf();
    //! \brief Trivial defaulted destructor.
    ~Mf() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Mf`.
    //! \return Newly-allocated `Mf` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief MFLI lock-in amplifier; decodes options via
//! `initializeSfcOptions` against `sfc::MfOption`.
class Mfli : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `MFLI` and family `MF`, mapping
    //!        `opts` through the MFLI `knownOptions` table.
    //! \param opts Raw MF-family options bitmask.
    explicit Mfli(unsigned long opts);              // vtable b09498
    //! \brief Trivial defaulted destructor.
    ~Mfli() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Mfli`.
    //! \return Newly-allocated `Mfli` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief MFIA impedance analyser; decodes options via
//! `initializeSfcOptions` against `sfc::MfOption` (the IA option bit
//! is recognised on this model).
class Mfia : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `MFIA` and family `MF`, mapping
    //!        `opts` through the MFIA `knownOptions` table.
    //! \param opts Raw MF-family options bitmask.
    explicit Mfia(unsigned long opts);              // vtable b094c0
    //! \brief Trivial defaulted destructor.
    ~Mfia() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Mfia`.
    //! \return Newly-allocated `Mfia` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// UHF family (DeviceFamily::UHF = 0x002)
// ---------------------------------------------------------------------------
//! \brief UHF family marker (zero-arg, no options).
class Uhf : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `UHF` and family `UHF`; option set is empty.
    Uhf();
    //! \brief Trivial defaulted destructor.
    ~Uhf() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Uhf`.
    //! \return Newly-allocated `Uhf` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief UHFLI lock-in amplifier; decodes options via
//! `initializeSfcOptions` against `sfc::UhfOption`.
class Uhfli : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `UHFLI` and family `UHF`, mapping
    //!        `opts` through the UHFLI `knownOptions` table.
    //! \param opts Raw UHF-family options bitmask.
    explicit Uhfli(unsigned long opts);             // vtable b095a0
    //! \brief Trivial defaulted destructor.
    ~Uhfli() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Uhfli`.
    //! \return Newly-allocated `Uhfli` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief UHF AWG variant; decodes options via `initializeSfcOptions`
//! against `sfc::UhfOption`.
class Uhfawg : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `UHFAWG` and family `UHF`, mapping
    //!        `opts` through the UHFAWG `knownOptions` table.
    //! \param opts Raw UHF-family options bitmask.
    explicit Uhfawg(unsigned long opts);            // vtable b095c8
    //! \brief Trivial defaulted destructor.
    ~Uhfawg() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Uhfawg`.
    //! \return Newly-allocated `Uhfawg` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief UHFQA quantum analyser; decodes options via
//! `initializeSfcOptions` against `sfc::UhfOption`.
class Uhfqa : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `UHFQA` and family `UHF`, mapping
    //!        `opts` through the UHFQA `knownOptions` table.
    //! \param opts Raw UHF-family options bitmask.
    explicit Uhfqa(unsigned long opts);             // vtable b095f0
    //! \brief Trivial defaulted destructor.
    ~Uhfqa() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Uhfqa`.
    //! \return Newly-allocated `Uhfqa` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief UHFIA impedance analyser; decodes options via
//! `initializeSfcOptions` against `sfc::UhfOption`.
class Uhfia : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `UHFIA` and family `UHF`, mapping
    //!        `opts` through the UHFIA `knownOptions` table.
    //! \param opts Raw UHF-family options bitmask.
    explicit Uhfia(unsigned long opts);             // vtable b09618
    //! \brief Trivial defaulted destructor.
    ~Uhfia() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Uhfia`.
    //! \return Newly-allocated `Uhfia` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// HDAWG family (DeviceFamily::HDAWG = 0x008)
// ---------------------------------------------------------------------------
//! \brief HDAWG family marker (zero-arg, no options).
class Hdawg : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HDAWG` and family `HDAWG`; option set is empty.
    Hdawg();
    //! \brief Trivial defaulted destructor.
    ~Hdawg() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hdawg`.
    //! \return Newly-allocated `Hdawg` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief HDAWG 4-channel arbitrary waveform generator; decodes
//! options via `initializeSfcOptions` against `sfc::HdawgOption`.
class Hdawg4 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HDAWG4` and family `HDAWG`,
    //!        mapping `opts` through the HDAWG4 `knownOptions` table.
    //! \param opts Raw HDAWG-family options bitmask.
    explicit Hdawg4(unsigned long opts);            // vtable b09728
    //! \brief Trivial defaulted destructor.
    ~Hdawg4() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hdawg4`.
    //! \return Newly-allocated `Hdawg4` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief HDAWG 8-channel arbitrary waveform generator; decodes
//! options via `initializeSfcOptions` against `sfc::HdawgOption`.
class Hdawg8 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HDAWG8` and family `HDAWG`,
    //!        mapping `opts` through the HDAWG8 `knownOptions` table.
    //! \param opts Raw HDAWG-family options bitmask.
    explicit Hdawg8(unsigned long opts);            // vtable b09750
    //! \brief Trivial defaulted destructor.
    ~Hdawg8() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hdawg8`.
    //! \return Newly-allocated `Hdawg8` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// SHF family (DeviceFamily::SHF = 0x010)
// ---------------------------------------------------------------------------
//! \brief SHF family marker; tests an inline FF bit on the option
//! mask rather than using an `initializeSfcOptions` table.
class Shf : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHF` and family `SHF`, building
    //!        the option set inline from the FF bit of `opts`.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shf(unsigned long opts);               // vtable b09808 (inline FF)
    //! \brief Trivial defaulted destructor.
    ~Shf() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shf`.
    //! \return Newly-allocated `Shf` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFQA 2-channel quantum analyser; decodes options via
//! `initializeSfcOptions` against `sfc::ShfOption`.
class Shfqa2 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFQA2` and family `SHF`, mapping
    //!        `opts` through the SHFQA2 `knownOptions` table.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfqa2(unsigned long opts);            // vtable b09830
    //! \brief Trivial defaulted destructor.
    ~Shfqa2() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfqa2`.
    //! \return Newly-allocated `Shfqa2` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFQA 4-channel quantum analyser; tests inline FF / PLUS /
//! LRT bits on the option mask rather than using an
//! `initializeSfcOptions` table.
class Shfqa4 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFQA4` and family `SHF`,
    //!        building the option set inline from the FF / PLUS /
    //!        LRT bits of `opts`.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfqa4(unsigned long opts);            // vtable b09858 (inline FF/PLUS/LRT)
    //! \brief Trivial defaulted destructor.
    ~Shfqa4() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfqa4`.
    //! \return Newly-allocated `Shfqa4` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFSG 2-channel signal generator; tests inline FF / RTR /
//! PLUS bits on the option mask.
class Shfsg2 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFSG2` and family `SHF`,
    //!        building the option set inline from the FF / RTR /
    //!        PLUS bits of `opts`.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfsg2(unsigned long opts);            // inline FF/RTR/PLUS
    //! \brief Trivial defaulted destructor.
    ~Shfsg2() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfsg2`.
    //! \return Newly-allocated `Shfsg2` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFSG 4-channel signal generator; tests inline FF / RTR /
//! PLUS bits on the option mask.
class Shfsg4 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFSG4` and family `SHF`,
    //!        building the option set inline from the FF / RTR /
    //!        PLUS bits of `opts`.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfsg4(unsigned long opts);            // inline FF/RTR/PLUS
    //! \brief Trivial defaulted destructor.
    ~Shfsg4() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfsg4`.
    //! \return Newly-allocated `Shfsg4` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFSG 8-channel signal generator; tests inline FF / RTR /
//! PLUS bits on the option mask.
class Shfsg8 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFSG8` and family `SHF`,
    //!        building the option set inline from the FF / RTR /
    //!        PLUS bits of `opts`.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfsg8(unsigned long opts);            // inline FF/RTR/PLUS
    //! \brief Trivial defaulted destructor.
    ~Shfsg8() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfsg8`.
    //! \return Newly-allocated `Shfsg8` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFQC quantum controller (QA + SG sequencer in one box);
//! decodes options via `initializeSfcOptions` against `sfc::ShfOption`.
class Shfqc : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFQC` and family `SHF`, mapping
    //!        `opts` through the SHFQC `knownOptions` table.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfqc(unsigned long opts);             // vtable b098f8
    //! \brief Trivial defaulted destructor.
    ~Shfqc() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfqc`.
    //! \return Newly-allocated `Shfqc` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFLI lock-in amplifier; decodes options via
//! `initializeSfcOptions` against `sfc::ShfOption`.
class Shfli : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFLI` and family `SHF`, mapping
    //!        `opts` through the SHFLI `knownOptions` table.
    //! \param opts Raw SHF-family options bitmask.
    explicit Shfli(unsigned long opts);             // vtable b09920
    //! \brief Trivial defaulted destructor.
    ~Shfli() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfli`.
    //! \return Newly-allocated `Shfli` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// SHFACC family (DeviceFamily::SHFACC = 0x080)
// ---------------------------------------------------------------------------
//! \brief SHFACC accessory unit; tests an inline FF bit on the option
//! mask.
class Shfacc : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFACC` and family `SHFACC`,
    //!        building the option set inline from the FF bit of `opts`.
    //! \param opts Raw SHFACC-family options bitmask.
    explicit Shfacc(unsigned long opts);            // vtable b09a50 (inline FF)
    //! \brief Trivial defaulted destructor.
    ~Shfacc() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfacc`.
    //! \return Newly-allocated `Shfacc` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFPPC2 2-channel parametric pump controller; tests an
//! inline FF bit on the option mask.
class Shfppc2 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFPPC2` and family `SHFACC`,
    //!        building the option set inline from the FF bit of `opts`.
    //! \param opts Raw SHFACC-family options bitmask.
    explicit Shfppc2(unsigned long opts);           // vtable b09a78 (inline FF)
    //! \brief Trivial defaulted destructor.
    ~Shfppc2() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfppc2`.
    //! \return Newly-allocated `Shfppc2` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief SHFPPC4 4-channel parametric pump controller; tests an
//! inline FF bit on the option mask.
class Shfppc4 : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `SHFPPC4` and family `SHFACC`,
    //!        building the option set inline from the FF bit of `opts`.
    //! \param opts Raw SHFACC-family options bitmask.
    explicit Shfppc4(unsigned long opts);           // vtable b09aa0 (inline FF)
    //! \brief Trivial defaulted destructor.
    ~Shfppc4() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Shfppc4`.
    //! \return Newly-allocated `Shfppc4` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// GHF family (DeviceFamily::GHF = 0x100) — uses sfc::ShfOption (no GhfOption)
// ---------------------------------------------------------------------------
//! \brief GHF family marker; tests an inline FF bit on the option
//! mask.
class Ghf : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `GHF` and family `GHF`, building
    //!        the option set inline from the FF bit of `opts`.
    //! \param opts Raw GHF-family options bitmask.
    explicit Ghf(unsigned long opts);               // vtable b09b58 (inline FF)
    //! \brief Trivial defaulted destructor.
    ~Ghf() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Ghf`.
    //! \return Newly-allocated `Ghf` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief GHFLI lock-in amplifier; decodes options via
//! `initializeSfcOptions` against `sfc::ShfOption` (the GHF family
//! reuses the SHF option enum).
class Ghfli : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `GHFLI` and family `GHF`, mapping
    //!        `opts` through the GHFLI `knownOptions` table (which
    //!        is keyed on `sfc::ShfOption`).
    //! \param opts Raw GHF-family options bitmask (uses SHF encoding).
    explicit Ghfli(unsigned long opts);             // vtable b09b80, knownOptions @ 0x96298c
    //! \brief Trivial defaulted destructor.
    ~Ghfli() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Ghfli`.
    //! \return Newly-allocated `Ghfli` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// PQSC family (DeviceFamily::PQSC = 0x020)
// ---------------------------------------------------------------------------
//! \brief PQSC programmable quantum system controller (zero-arg, no
//! options).
class Pqsc : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `PQSC` and family `PQSC`; option set is empty.
    Pqsc();
    //! \brief Trivial defaulted destructor.
    ~Pqsc() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Pqsc`.
    //! \return Newly-allocated `Pqsc` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// QHUB family (DeviceFamily::QHUB = 0x200)
// ---------------------------------------------------------------------------
//! \brief QHUB quantum hub (zero-arg, no options).
class Qhub : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `QHUB` and family `QHUB`; option set is empty.
    Qhub();
    //! \brief Trivial defaulted destructor.
    ~Qhub() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Qhub`.
    //! \return Newly-allocated `Qhub` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// HWMOCK family (DeviceFamily::HWMOCK = 0x040)
// ---------------------------------------------------------------------------
//! \brief Hardware-mock device used for testing (zero-arg, no
//! options).
class Hwmock : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `HWMOCK` and family `HWMOCK`; option set is empty.
    Hwmock();
    //! \brief Trivial defaulted destructor.
    ~Hwmock() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Hwmock`.
    //! \return Newly-allocated `Hwmock` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// VHF family (DeviceFamily::VHF = 0x400)
// ---------------------------------------------------------------------------
//! \brief VHF family marker; tests an inline FF bit on the option
//! mask.
class Vhf : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `VHF` and family `VHF`, building
    //!        the option set inline from the FF bit of `opts`.
    //! \param opts Raw VHF-family options bitmask.
    explicit Vhf(unsigned long opts);               // vtable b09db8 (inline FF)
    //! \brief Trivial defaulted destructor.
    ~Vhf() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Vhf`.
    //! \return Newly-allocated `Vhf` copy.
    DeviceTypeImpl* clone() const override;
};

//! \brief VHFLI lock-in amplifier; decodes options via
//! `initializeSfcOptions` against `sfc::VhfOption`.
class Vhfli : public DeviceTypeImpl {
public:
    //! \brief Constructs with code `VHFLI` and family `VHF`, mapping
    //!        `opts` through the VHFLI `knownOptions` table.
    //! \param opts Raw VHF-family options bitmask.
    explicit Vhfli(unsigned long opts);             // vtable b09de0
    //! \brief Trivial defaulted destructor.
    ~Vhfli() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is `Vhfli`.
    //! \return Newly-allocated `Vhfli` copy.
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// detail::UnknownDevice — special "unknown" subclass used by the parser
// when a device-type string isn't recognised. Stores the synthetic sentinel
// values code=33 and family=0x800 (both outside the documented enum
// ranges). Vtable @ .rodata 0xb09038.
//
// Also produced by both `UnknownDeviceTypeFactory::makeDefault()` and
// `UnknownDeviceTypeFactory::doMakeDeviceType(unsigned long)`.
// ---------------------------------------------------------------------------
//! \brief Sentinel `DeviceTypeImpl` subclass returned for device-type
//! strings the parser does not recognise; carries `code = 33`
//! ("unknown") and a synthetic family value outside the named
//! `DeviceFamily` range.
class UnknownDevice : public DeviceTypeImpl {
public:
    //! \brief Constructs with the synthetic sentinel code `33`
    //!        ("unknown") and family `0x800`; option set is empty.
    UnknownDevice();                                // @ 0x2d3320, vtable b09038
    //! \brief Trivial defaulted destructor.
    ~UnknownDevice() override;
    //! \brief Returns a heap-allocated copy whose dynamic type is
    //!        `UnknownDevice`, preserving the sentinel identity.
    //! \return Newly-allocated `UnknownDevice` copy.
    DeviceTypeImpl* clone() const override;
};

}  // namespace detail
}  // namespace zhinst
