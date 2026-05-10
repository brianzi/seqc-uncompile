// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_factories.hpp
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

#include "zhinst/device/device_subclasses.hpp"
#include "zhinst/device/device_type.hpp"

#include <memory>

namespace zhinst {

// Device factory option bit constants live in the anonymous namespace inside
// device_factories.cpp (kSubtypeMask, kSubtype1..kSubtype4 etc.).  An older
// duplicate `namespace DeviceOpts { ... }` lived here but was never referenced
// from anywhere — removed in Phase R (IF-121).

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
//! \brief Abstract factory base for constructing concrete
//! `detail::DeviceTypeImpl` instances from an options bitmask.
//!
//! Public `makeDefault()` / `makeDeviceType(opts)` helpers dispatch
//! through the vtable into the family-specific
//! `doMakeDefault()` / `doMakeDeviceType(opts)` overrides.  One
//! concrete subclass exists per `DeviceFamily` (Hf2Factory, MfFactory,
//! UhfFactory, HdawgFactory, ShfFactory, ShfaccFactory, GhfFactory,
//! PqscFactory, QhubFactory, HwmockFactory, VhfFactory) plus two
//! special factories (`NoDeviceTypeFactory` for the Unknown family and
//! `UnknownDeviceTypeFactory` for unrecognised family values).  The
//! correct factory for a given family is produced by
//! `makeDeviceFamilyFactory`.
class DeviceFamilyFactory {
public:
    //! \brief Polymorphic destructor for the factory hierarchy.
    virtual ~DeviceFamilyFactory();

    //! \brief Constructs the family's default device instance (no
    //!        options) by dispatching to `doMakeDefault()`.
    //! \return Owning pointer to a freshly-allocated default device.
    // @ 0x2e0590 — calls [vtable+0x10] = doMakeDefault().
    std::unique_ptr<DeviceTypeImpl> makeDefault();

    //! \brief Constructs a device for `opts` by dispatching to
    //!        `doMakeDeviceType(opts)`; the derived factory uses the
    //!        family's selector bits to pick the concrete subclass
    //!        and the remaining bits as the options bitmask.
    //! \param opts Raw per-family options bitmask.
    //! \return Owning pointer to a freshly-allocated device.
    // @ 0x2e05b0 — calls [vtable+0x18] = doMakeDeviceType(opts).
    std::unique_ptr<DeviceTypeImpl> makeDeviceType(unsigned long opts);

    //! \brief Zero-arg overload equivalent to `makeDefault()`.
    //! \return Owning pointer to a freshly-allocated default device.
    // Zero-arg overload — same as makeDefault() in the binary.
    std::unique_ptr<DeviceTypeImpl> makeDeviceType();

protected:
    //! \brief Family-specific implementation of the default-device
    //!        construction; overridden by every concrete factory.
    //! \return Owning pointer to a freshly-allocated default device.
    virtual std::unique_ptr<DeviceTypeImpl> doMakeDefault() = 0;
    //! \brief Family-specific implementation of the options-aware
    //!        construction; overridden by every concrete factory.
    //! \param opts Raw per-family options bitmask.
    //! \return Owning pointer to a freshly-allocated device.
    virtual std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long opts) = 0;
};

// ---------------------------------------------------------------------------
// NoDeviceTypeFactory — used for DeviceFamily::Unknown (value 0).
// Both makeDefault() and doMakeDeviceType() return a new
// default-constructed DeviceTypeImpl (i.e., the base class with
// code=Unknown, family=Unknown). Vtable @ 0xb092d8.
// ---------------------------------------------------------------------------
//! \brief Factory used for `DeviceFamily::Unknown`; both
//! `makeDefault()` and `makeDeviceType(opts)` produce a plain
//! base-`DeviceTypeImpl` carrying `code = Unknown` and `family = Unknown`.
class NoDeviceTypeFactory : public DeviceFamilyFactory {
public:
    //! \brief Trivial defaulted destructor.
    ~NoDeviceTypeFactory() override;
    //! \brief Returns a default-constructed base `DeviceTypeImpl`
    //!        (code `Unknown`, family `Unknown`).
    //! \return Owning pointer to a freshly-allocated default device.
    std::unique_ptr<DeviceTypeImpl> makeDefault();          // per-subclass override
protected:
    //! \brief Family-specific default: returns a default-constructed
    //!        base `DeviceTypeImpl`.
    //! \return Owning pointer to a freshly-allocated default device.
    std::unique_ptr<DeviceTypeImpl> doMakeDefault() override;          // @ 0x2e0700
    //! \brief Family-specific options-aware constructor; ignores
    //!        `opts` and returns a default-constructed base
    //!        `DeviceTypeImpl`.
    //! \return Owning pointer to a freshly-allocated default device.
    std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long) override;  // @ 0x2e0730
};

// ---------------------------------------------------------------------------
// UnknownDeviceTypeFactory — catch-all for any DeviceFamily value not
// in the documented enum. Both makeDefault() and doMakeDeviceType()
// return a new UnknownDevice. Vtable @ 0xb09310.
// ---------------------------------------------------------------------------
//! \brief Catch-all factory used when a `DeviceFamily` value is not
//! one of the documented one-hot bits; both `makeDefault()` and
//! `makeDeviceType(opts)` produce an `UnknownDevice` instance.
class UnknownDeviceTypeFactory : public DeviceFamilyFactory {
public:
    //! \brief Trivial defaulted destructor.
    ~UnknownDeviceTypeFactory() override;
    //! \brief Returns a freshly-allocated `UnknownDevice` instance.
    //! \return Owning pointer to a new `UnknownDevice`.
    std::unique_ptr<DeviceTypeImpl> makeDefault();          // per-subclass override
protected:
    //! \brief Family-specific default: returns a freshly-allocated
    //!        `UnknownDevice` instance.
    //! \return Owning pointer to a new `UnknownDevice`.
    std::unique_ptr<DeviceTypeImpl> doMakeDefault() override;          // @ 0x2e0760
    //! \brief Family-specific options-aware constructor; ignores
    //!        `opts` and returns a freshly-allocated `UnknownDevice`.
    //! \return Owning pointer to a new `UnknownDevice`.
    std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long) override;  // @ 0x2e07b0
};

// ---------------------------------------------------------------------------
// Per-family factory declarations. All 8 bytes (vptr only). All
// constructed by `makeDeviceFamilyFactory(DeviceFamily)`.
// ---------------------------------------------------------------------------
//! \brief Declares a concrete per-family factory class deriving from
//!        `DeviceFamilyFactory`.
//!
//! Every expansion is structurally identical: an empty-state class
//! whose virtual `doMakeDefault()` / `doMakeDeviceType(opts)`
//! overrides build the family's default device or — when the family
//! has subtype variants — pick the concrete subclass from the
//! subtype selector bits in `opts`.  Out-of-line bodies live in
//! `device_factories.cpp`; the subtype-selector scheme for each
//! family is documented in the file-level banner above.
//!
//! \param NAME Identifier of the generated factory class.
#define ZHINST_DECLARE_FACTORY(NAME)                                       \
    class NAME : public DeviceFamilyFactory {                              \
    public:                                                                \
        /*! \brief Trivial defaulted destructor. */                        \
        ~NAME() override;                                                  \
        /*! \brief Constructs the family's default device (no             \
         *         options) by dispatching to `doMakeDefault()`.          \
         *  \return Owning pointer to the freshly-allocated default       \
         *          device.                                               \
         */                                                                \
        std::unique_ptr<DeviceTypeImpl> makeDefault();                     \
    protected:                                                             \
        /*! \brief Family-specific default-device construction.           \
         *  \return Owning pointer to a freshly-allocated default device. \
         */                                                                \
        std::unique_ptr<DeviceTypeImpl> doMakeDefault() override;          \
        /*! \brief Family-specific options-aware construction; selects   \
         *         the concrete subclass from the family's subtype       \
         *         selector bits and uses the remaining bits as the      \
         *         per-family option bitmask.                            \
         *  \param opts Raw per-family options bitmask.                  \
         *  \return Owning pointer to the freshly-allocated device.      \
         */                                                                \
        std::unique_ptr<DeviceTypeImpl> doMakeDeviceType(unsigned long opts) override; \
    }

//! \brief Factory for `DeviceFamily::HF2` (subtypes: `Hf2`, `Hf2li`, `Hf2is`).
ZHINST_DECLARE_FACTORY(Hf2Factory);     // vtable b093c0
//! \brief Factory for `DeviceFamily::MF` (subtypes: `Mf`, `Mfli`, `Mfia`).
ZHINST_DECLARE_FACTORY(MfFactory);      // vtable b094c8
//! \brief Factory for `DeviceFamily::UHF` (subtypes: `Uhf`, `Uhfli`,
//!        `Uhfawg`, `Uhfqa`, `Uhfia`).
ZHINST_DECLARE_FACTORY(UhfFactory);     // vtable b09620
//! \brief Factory for `DeviceFamily::HDAWG` (subtypes: `Hdawg`,
//!        `Hdawg4`, `Hdawg8`).
ZHINST_DECLARE_FACTORY(HdawgFactory);   // vtable b09758
//! \brief Factory for `DeviceFamily::SHF` (subtypes: `Shf`, `Shfqa2`,
//!        `Shfqa4`, `Shfsg2`, `Shfsg4`, `Shfsg8`, `Shfqc`, `Shfli`).
ZHINST_DECLARE_FACTORY(ShfFactory);     // vtable b09928
//! \brief Factory for `DeviceFamily::SHFACC` (subtypes: `Shfacc`,
//!        `Shfppc2`, `Shfppc4`).
ZHINST_DECLARE_FACTORY(ShfaccFactory);  // vtable b09aa8
//! \brief Factory for `DeviceFamily::GHF` (subtypes: `Ghf`, `Ghfli`).
ZHINST_DECLARE_FACTORY(GhfFactory);     // vtable b09b88
//! \brief Factory for `DeviceFamily::PQSC` (single subtype: `Pqsc`).
ZHINST_DECLARE_FACTORY(PqscFactory);    // vtable b09c28
//! \brief Factory for `DeviceFamily::QHUB` (single subtype: `Qhub`).
ZHINST_DECLARE_FACTORY(QhubFactory);    // vtable b09cb0
//! \brief Factory for `DeviceFamily::HWMOCK` (single subtype: `Hwmock`).
ZHINST_DECLARE_FACTORY(HwmockFactory);  // vtable b09d38
//! \brief Factory for `DeviceFamily::VHF` (subtypes: `Vhf`, `Vhfli`).
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
