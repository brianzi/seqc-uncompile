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
class Hf2 : public DeviceTypeImpl {
public:
    Hf2();                                          // @ 0x2e0830, vtable b09368
    ~Hf2() override;
    DeviceTypeImpl* clone() const override;
};

class Hf2li : public DeviceTypeImpl {
public:
    explicit Hf2li(unsigned long opts);             // vtable b09390
    ~Hf2li() override;
    DeviceTypeImpl* clone() const override;
};

class Hf2is : public DeviceTypeImpl {
public:
    explicit Hf2is(unsigned long opts);             // vtable b093b8
    ~Hf2is() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// MF family (DeviceFamily::MF = 0x004)
// ---------------------------------------------------------------------------
class Mf : public DeviceTypeImpl {
public:
    Mf();
    ~Mf() override;
    DeviceTypeImpl* clone() const override;
};

class Mfli : public DeviceTypeImpl {
public:
    explicit Mfli(unsigned long opts);              // vtable b09498
    ~Mfli() override;
    DeviceTypeImpl* clone() const override;
};

class Mfia : public DeviceTypeImpl {
public:
    explicit Mfia(unsigned long opts);              // vtable b094c0
    ~Mfia() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// UHF family (DeviceFamily::UHF = 0x002)
// ---------------------------------------------------------------------------
class Uhf : public DeviceTypeImpl {
public:
    Uhf();
    ~Uhf() override;
    DeviceTypeImpl* clone() const override;
};

class Uhfli : public DeviceTypeImpl {
public:
    explicit Uhfli(unsigned long opts);             // vtable b095a0
    ~Uhfli() override;
    DeviceTypeImpl* clone() const override;
};

class Uhfawg : public DeviceTypeImpl {
public:
    explicit Uhfawg(unsigned long opts);            // vtable b095c8
    ~Uhfawg() override;
    DeviceTypeImpl* clone() const override;
};

class Uhfqa : public DeviceTypeImpl {
public:
    explicit Uhfqa(unsigned long opts);             // vtable b095f0
    ~Uhfqa() override;
    DeviceTypeImpl* clone() const override;
};

class Uhfia : public DeviceTypeImpl {
public:
    explicit Uhfia(unsigned long opts);             // vtable b09618
    ~Uhfia() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// HDAWG family (DeviceFamily::HDAWG = 0x008)
// ---------------------------------------------------------------------------
class Hdawg : public DeviceTypeImpl {
public:
    Hdawg();
    ~Hdawg() override;
    DeviceTypeImpl* clone() const override;
};

class Hdawg4 : public DeviceTypeImpl {
public:
    explicit Hdawg4(unsigned long opts);            // vtable b09728
    ~Hdawg4() override;
    DeviceTypeImpl* clone() const override;
};

class Hdawg8 : public DeviceTypeImpl {
public:
    explicit Hdawg8(unsigned long opts);            // vtable b09750
    ~Hdawg8() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// SHF family (DeviceFamily::SHF = 0x010)
// ---------------------------------------------------------------------------
class Shf : public DeviceTypeImpl {
public:
    explicit Shf(unsigned long opts);               // vtable b09808 (inline FF)
    ~Shf() override;
    DeviceTypeImpl* clone() const override;
};

class Shfqa2 : public DeviceTypeImpl {
public:
    explicit Shfqa2(unsigned long opts);            // vtable b09830
    ~Shfqa2() override;
    DeviceTypeImpl* clone() const override;
};

class Shfqa4 : public DeviceTypeImpl {
public:
    explicit Shfqa4(unsigned long opts);            // vtable b09858 (inline FF/PLUS/LRT)
    ~Shfqa4() override;
    DeviceTypeImpl* clone() const override;
};

class Shfsg2 : public DeviceTypeImpl {
public:
    explicit Shfsg2(unsigned long opts);            // inline FF/RTR/PLUS
    ~Shfsg2() override;
    DeviceTypeImpl* clone() const override;
};

class Shfsg4 : public DeviceTypeImpl {
public:
    explicit Shfsg4(unsigned long opts);            // inline FF/RTR/PLUS
    ~Shfsg4() override;
    DeviceTypeImpl* clone() const override;
};

class Shfsg8 : public DeviceTypeImpl {
public:
    explicit Shfsg8(unsigned long opts);            // inline FF/RTR/PLUS
    ~Shfsg8() override;
    DeviceTypeImpl* clone() const override;
};

class Shfqc : public DeviceTypeImpl {
public:
    explicit Shfqc(unsigned long opts);             // vtable b098f8
    ~Shfqc() override;
    DeviceTypeImpl* clone() const override;
};

class Shfli : public DeviceTypeImpl {
public:
    explicit Shfli(unsigned long opts);             // vtable b09920
    ~Shfli() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// SHFACC family (DeviceFamily::SHFACC = 0x080)
// ---------------------------------------------------------------------------
class Shfacc : public DeviceTypeImpl {
public:
    explicit Shfacc(unsigned long opts);            // vtable b09a50 (inline FF)
    ~Shfacc() override;
    DeviceTypeImpl* clone() const override;
};

class Shfppc2 : public DeviceTypeImpl {
public:
    explicit Shfppc2(unsigned long opts);           // vtable b09a78 (inline FF)
    ~Shfppc2() override;
    DeviceTypeImpl* clone() const override;
};

class Shfppc4 : public DeviceTypeImpl {
public:
    explicit Shfppc4(unsigned long opts);           // vtable b09aa0 (inline FF)
    ~Shfppc4() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// GHF family (DeviceFamily::GHF = 0x100) — uses sfc::ShfOption (no GhfOption)
// ---------------------------------------------------------------------------
class Ghf : public DeviceTypeImpl {
public:
    explicit Ghf(unsigned long opts);               // vtable b09b58 (inline FF)
    ~Ghf() override;
    DeviceTypeImpl* clone() const override;
};

class Ghfli : public DeviceTypeImpl {
public:
    explicit Ghfli(unsigned long opts);             // vtable b09b80, knownOptions @ 0x96298c
    ~Ghfli() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// PQSC family (DeviceFamily::PQSC = 0x020)
// ---------------------------------------------------------------------------
class Pqsc : public DeviceTypeImpl {
public:
    Pqsc();
    ~Pqsc() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// QHUB family (DeviceFamily::QHUB = 0x200)
// ---------------------------------------------------------------------------
class Qhub : public DeviceTypeImpl {
public:
    Qhub();
    ~Qhub() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// HWMOCK family (DeviceFamily::HWMOCK = 0x040)
// ---------------------------------------------------------------------------
class Hwmock : public DeviceTypeImpl {
public:
    Hwmock();
    ~Hwmock() override;
    DeviceTypeImpl* clone() const override;
};

// ---------------------------------------------------------------------------
// VHF family (DeviceFamily::VHF = 0x400)
// ---------------------------------------------------------------------------
class Vhf : public DeviceTypeImpl {
public:
    explicit Vhf(unsigned long opts);               // vtable b09db8 (inline FF)
    ~Vhf() override;
    DeviceTypeImpl* clone() const override;
};

class Vhfli : public DeviceTypeImpl {
public:
    explicit Vhfli(unsigned long opts);             // vtable b09de0
    ~Vhfli() override;
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
class UnknownDevice : public DeviceTypeImpl {
public:
    UnknownDevice();                                // @ 0x2d3320, vtable b09038
    ~UnknownDevice() override;
    DeviceTypeImpl* clone() const override;
};

}  // namespace detail
}  // namespace zhinst
