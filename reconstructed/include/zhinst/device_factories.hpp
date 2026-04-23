// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_factories.hpp — Phase 14b-ii-b2
//
// Factory classes used to construct `detail::DeviceTypeImpl`-derived
// device objects from a (DeviceFamily, options-bitmask) pair. All
// factories are 8 bytes (vptr only — no instance data).
//
// Class hierarchy:
//
//   detail::DeviceFamilyFactory          (abstract base, 8B)
//     ├── detail::NoDeviceTypeFactory    (placeholder for Unknown family)
//     ├── detail::UnknownDeviceTypeFactory (catch-all for unknown family value)
//     ├── detail::Hf2Factory
//     ├── detail::MfFactory
//     ├── detail::UhfFactory
//     ├── detail::HdawgFactory
//     ├── detail::ShfFactory
//     ├── detail::ShfaccFactory
//     ├── detail::GhfFactory
//     ├── detail::PqscFactory
//     ├── detail::QhubFactory
//     ├── detail::HwmockFactory
//     └── detail::VhfFactory
//
// Vtable layout (16-byte RTTI prologue, then methods):
//   +0x00  RTTI offset_to_top (0)
//   +0x08  RTTI typeinfo*
//   +0x10  ~Factory()         (in-charge dtor)
//   +0x18  ~Factory()         (deleting dtor, 2nd entry)
//   +0x20  doMakeDefault()           (called by base makeDefault)
//   +0x28  doMakeDeviceType(opts)    (called by base makeDeviceType)
//
// Wait — vtable indices in the trampolines are [+0x10] and [+0x18]
// after `mov rax, [rsi]`. The vtable pointer in the object points at
// the FIRST method slot (libc++ ABI: `*(rsi)` already skips the
// RTTI prologue), so:
//   [vtable+0x00] = ~Factory (1st)
//   [vtable+0x08] = ~Factory (2nd)
//   [vtable+0x10] = doMakeDefault
//   [vtable+0x18] = doMakeDeviceType
//
// `makeDefault()` and `makeDeviceType(opts)` are non-virtual public
// helpers on the base that simply call through the vtable into the
// derived. Per modern C++ practice we keep them virtual (with default
// implementations on the base that throw), but the on-the-wire shape
// matches what the binary does: out-arg-pointer NRVO of an 8-byte
// `unique_ptr<DeviceTypeImpl>`-equivalent.
//
// Per-family subtype-selector bit schemes (decoded in 14b-ii-b2):
//   Hf2Factory    : opts & 0x1c0; 0x80→Hf2is, 0x40→Hf2li, else→Hf2
//   MfFactory     : opts & 0x1c0; 0x80→Mfia,  0x40→Mfli,  else→Mf
//   UhfFactory    : (opts & 0x1c0) >> 6 in {0..3} — 4-way jump table
//                   slot 0 (0x40)→Uhfli, slot 1 (0x80)→Uhfawg,
//                   slot 2 (0xc0)→Uhfqa, slot 3 (0x100)→Uhfia,
//                   else→Uhf
//   HdawgFactory  : opts & 0x1c0; 0x80→Hdawg8, 0x40→Hdawg4, else→Hdawg
//   ShfFactory    : (opts >> 6) & 7 — 8-way jump table, all slots used
//                   0→Shf, 1→Shfqa2, 2→Shfqa4, 3→Shfsg4,
//                   4→Shfsg8, 5→Shfqc, 6→Shfli, 7→Shfsg2
//   ShfaccFactory : opts & 0x1c0; 0x80→Shfppc4, 0x40→Shfppc2,
//                   else→Shfacc (NB: no separate base — Shfacc IS
//                   the default subclass)
//   GhfFactory    : opts & 0x1c0; 0x40→Ghfli, else→Ghf
//   PqscFactory   : ignores opts; always Pqsc
//   QhubFactory   : ignores opts; always Qhub
//   HwmockFactory : ignores opts; always Hwmock
//   VhfFactory    : opts & 0x1c0; 0x40→Vhfli, else→Vhf
// ============================================================================
#pragma once

#include "zhinst/device_subclasses.hpp"
#include "zhinst/device_type.hpp"

#include <memory>

namespace zhinst {
namespace detail {

// ---------------------------------------------------------------------------
// DeviceFamilyFactory — abstract base. 8 bytes (vptr only).
//
// Public non-virtual API:
//   makeDefault()             — returns a new default-options instance
//   makeDeviceType(opts)      — returns a new instance with given opts
//
// Implementation: both helpers immediately call through the vtable into
// the derived class's `doMakeDefault()` / `doMakeDeviceType(opts)`
// virtual methods.
// ---------------------------------------------------------------------------
class DeviceFamilyFactory {
public:
    virtual ~DeviceFamilyFactory();

    // @ 0x2e0590 — calls [vtable+0x10] = doMakeDefault().
    std::unique_ptr<DeviceTypeImpl> makeDefault();

    // @ 0x2e05b0 — calls [vtable+0x18] = doMakeDeviceType(opts).
    std::unique_ptr<DeviceTypeImpl> makeDeviceType(unsigned long opts);

protected:
    virtual std::unique_ptr<DeviceTypeImpl> doMakeDefault() = 0;
    virtual std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long opts) = 0;
};

// ---------------------------------------------------------------------------
// NoDeviceTypeFactory — used for DeviceFamily::Unknown (value 0).
// Both makeDefault() and doMakeDeviceType() return a new
// default-constructed DeviceTypeImpl (i.e., the base class with
// code=Unknown, family=Unknown). Vtable @ 0xb092d8.
// ---------------------------------------------------------------------------
class NoDeviceTypeFactory : public DeviceFamilyFactory {
public:
    ~NoDeviceTypeFactory() override;
protected:
    std::unique_ptr<DeviceTypeImpl> doMakeDefault() override;          // @ 0x2e0700
    std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long) override;  // @ 0x2e0730
};

// ---------------------------------------------------------------------------
// UnknownDeviceTypeFactory — catch-all for any DeviceFamily value not
// in the documented enum. Both makeDefault() and doMakeDeviceType()
// return a new UnknownDevice. Vtable @ 0xb09310.
// ---------------------------------------------------------------------------
class UnknownDeviceTypeFactory : public DeviceFamilyFactory {
public:
    ~UnknownDeviceTypeFactory() override;
protected:
    std::unique_ptr<DeviceTypeImpl> doMakeDefault() override;          // @ 0x2e0760
    std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long) override;  // @ 0x2e07b0
};

// ---------------------------------------------------------------------------
// Per-family factory declarations. All 8 bytes (vptr only). All
// constructed by `makeDeviceFamilyFactory(DeviceFamily)`.
// ---------------------------------------------------------------------------
#define ZHINST_DECLARE_FACTORY(NAME)                                       \
    class NAME : public DeviceFamilyFactory {                              \
    public:                                                                \
        ~NAME() override;                                                  \
    protected:                                                             \
        std::unique_ptr<DeviceTypeImpl> doMakeDefault() override;          \
        std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long opts) override; \
    }

ZHINST_DECLARE_FACTORY(Hf2Factory);     // vtable b093c0
ZHINST_DECLARE_FACTORY(MfFactory);      // vtable b094c8
ZHINST_DECLARE_FACTORY(UhfFactory);     // vtable b09620
ZHINST_DECLARE_FACTORY(HdawgFactory);   // vtable b09758
ZHINST_DECLARE_FACTORY(ShfFactory);     // vtable b09928
ZHINST_DECLARE_FACTORY(ShfaccFactory);  // vtable b09aa8
ZHINST_DECLARE_FACTORY(GhfFactory);     // vtable b09b88
ZHINST_DECLARE_FACTORY(PqscFactory);    // vtable b09c28
ZHINST_DECLARE_FACTORY(QhubFactory);    // vtable b09cb0
ZHINST_DECLARE_FACTORY(HwmockFactory);  // vtable b09d38
ZHINST_DECLARE_FACTORY(VhfFactory);     // vtable b09de8

#undef ZHINST_DECLARE_FACTORY

// ---------------------------------------------------------------------------
// makeDeviceFamilyFactory — factory-of-factories.
// @ 0x2e05d0
//
// Switches on the family value (one-hot DeviceFamily) and constructs
// the appropriate factory subclass; falls back to UnknownDeviceTypeFactory
// for any value not in the documented enum.
//
// The binary returns by writing a single 8-byte pointer to the output
// argument; the C++ source-level shape is `unique_ptr` returned via
// NRVO.
// ---------------------------------------------------------------------------
std::unique_ptr<DeviceFamilyFactory>
makeDeviceFamilyFactory(DeviceFamily family);

}  // namespace detail
}  // namespace zhinst
