// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_factories.cpp
//
// All 13 factory classes (abstract base, NoDeviceTypeFactory,
// UnknownDeviceTypeFactory, 11 per-family factories) plus the
// makeDeviceFamilyFactory(DeviceFamily) dispatcher.
//
// Per-family subtype-selector bit schemes are documented at the top of
// device_factories.hpp and in reconstructed/notes/device_type.md.
// ============================================================================

#include "zhinst/device/device_factories.hpp"
#include "zhinst/device/device_subclasses.hpp"

#include <memory>

namespace zhinst {
namespace detail {

// Subtype selector constants — extracted from binary .rodata selector masks.
// The `opts` bitfield encodes the device sub-variant in bits [8:6] (mask 0x1C0).
// Values select specific device subclasses within a family.
namespace {
constexpr unsigned long kSubtypeMask = 0x1C0ul;  // bits [8:6]
constexpr unsigned long kSubtype1    = 0x040ul;  // slot 1 (LI variants)
constexpr unsigned long kSubtype2    = 0x080ul;  // slot 2 (IS/AWG/8 variants)
constexpr unsigned long kSubtype3    = 0x0C0ul;  // slot 3 (QA variants)
constexpr unsigned long kSubtype4    = 0x100ul;  // slot 4 (IA variants)
} // anonymous namespace

// ===========================================================================
// DeviceFamilyFactory — abstract base
// ===========================================================================

DeviceFamilyFactory::~DeviceFamilyFactory() = default;  // @ 0x2e0810

// @ 0x2e0590 — base trampoline.
std::unique_ptr<DeviceTypeImpl> DeviceFamilyFactory::makeDefault() {
    return doMakeDefault();
}

// @ 0x2e05b0 — base trampoline.
std::unique_ptr<DeviceTypeImpl> DeviceFamilyFactory::makeDeviceType(unsigned long opts) {
    return doMakeDeviceType(opts);
}

// Zero-arg overload — same as makeDefault().
std::unique_ptr<DeviceTypeImpl> DeviceFamilyFactory::makeDeviceType() {
    return doMakeDefault();
}

// ===========================================================================
// NoDeviceTypeFactory — produces base DeviceTypeImpl (default-constructed).
// Vtable @ 0xb092d8.
// ===========================================================================

NoDeviceTypeFactory::~NoDeviceTypeFactory() = default;  // @ 0x2e0800

std::unique_ptr<DeviceTypeImpl> NoDeviceTypeFactory::makeDefault() {
    return doMakeDefault();
}

// @ 0x2e0700
std::unique_ptr<DeviceTypeImpl> NoDeviceTypeFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new DeviceTypeImpl());
}

// @ 0x2e0730
std::unique_ptr<DeviceTypeImpl> NoDeviceTypeFactory::doMakeDeviceType(unsigned long /*opts*/) {
    return std::unique_ptr<DeviceTypeImpl>(new DeviceTypeImpl());
}

// ===========================================================================
// UnknownDeviceTypeFactory — produces UnknownDevice.
// Vtable @ 0xb09310.
// ===========================================================================

UnknownDeviceTypeFactory::~UnknownDeviceTypeFactory() = default;  // @ 0x2e0820

std::unique_ptr<DeviceTypeImpl> UnknownDeviceTypeFactory::makeDefault() {
    return doMakeDefault();
}

// @ 0x2e0760
std::unique_ptr<DeviceTypeImpl> UnknownDeviceTypeFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new UnknownDevice());
}

// @ 0x2e07b0
std::unique_ptr<DeviceTypeImpl> UnknownDeviceTypeFactory::doMakeDeviceType(unsigned long /*opts*/) {
    return std::unique_ptr<DeviceTypeImpl>(new UnknownDevice());
}

// ===========================================================================
// Per-family factories
// ===========================================================================

// ---- HF2 ----
Hf2Factory::~Hf2Factory() = default;                    // @ 0x2e0ae0
std::unique_ptr<DeviceTypeImpl> Hf2Factory::makeDefault() { return doMakeDefault(); }

// @ 0x2e09e0 — inlines `new Hf2()` (no opts).
std::unique_ptr<DeviceTypeImpl> Hf2Factory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Hf2());
}

// @ 0x2e0a40 — selector mask 0x1c0; 0x80→Hf2is, 0x40→Hf2li, else→Hf2.
std::unique_ptr<DeviceTypeImpl> Hf2Factory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    if (sub == kSubtype2) return std::unique_ptr<DeviceTypeImpl>(new Hf2is(opts));
    if (sub == kSubtype1) return std::unique_ptr<DeviceTypeImpl>(new Hf2li(opts));
    return std::unique_ptr<DeviceTypeImpl>(new Hf2());
}

// ---- MF ----
MfFactory::~MfFactory() = default;                      // @ 0x2e10c0
std::unique_ptr<DeviceTypeImpl> MfFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e0fc0
std::unique_ptr<DeviceTypeImpl> MfFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Mf());
}

// @ 0x2e1020 — selector mask 0x1c0; 0x80→Mfia, 0x40→Mfli, else→Mf.
std::unique_ptr<DeviceTypeImpl> MfFactory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    if (sub == kSubtype2) return std::unique_ptr<DeviceTypeImpl>(new Mfia(opts));
    if (sub == kSubtype1) return std::unique_ptr<DeviceTypeImpl>(new Mfli(opts));
    return std::unique_ptr<DeviceTypeImpl>(new Mf());
}

// ---- UHF ----
UhfFactory::~UhfFactory() = default;                    // @ 0x2e18b0
std::unique_ptr<DeviceTypeImpl> UhfFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e1780
std::unique_ptr<DeviceTypeImpl> UhfFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Uhf());
}

// @ 0x2e17e0 — 4-way jump table over `((opts & 0x1c0) - 0x40) >> 6`:
//   slot 0 (0x40)  → Uhfli
//   slot 1 (0x80)  → Uhfawg
//   slot 2 (0xc0)  → Uhfqa
//   slot 3 (0x100) → Uhfia
//   else (incl. 0) → Uhf base
// (Selector slot order decoded from .rodata jump table @ 0x9624e0.)
std::unique_ptr<DeviceTypeImpl> UhfFactory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    switch (sub) {
        case kSubtype1:  return std::unique_ptr<DeviceTypeImpl>(new Uhfli(opts));
        case kSubtype2:  return std::unique_ptr<DeviceTypeImpl>(new Uhfawg(opts));
        case kSubtype3:  return std::unique_ptr<DeviceTypeImpl>(new Uhfqa(opts));
        case kSubtype4:  return std::unique_ptr<DeviceTypeImpl>(new Uhfia(opts));
        default:      return std::unique_ptr<DeviceTypeImpl>(new Uhf());
    }
}

// ---- HDAWG ----
HdawgFactory::~HdawgFactory() = default;                // @ 0x2e2190
std::unique_ptr<DeviceTypeImpl> HdawgFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e2090
std::unique_ptr<DeviceTypeImpl> HdawgFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Hdawg());
}

// @ 0x2e20f0 — selector mask 0x1c0; 0x80→Hdawg8, 0x40→Hdawg4, else→Hdawg.
std::unique_ptr<DeviceTypeImpl> HdawgFactory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    if (sub == kSubtype2) return std::unique_ptr<DeviceTypeImpl>(new Hdawg8(opts));
    if (sub == kSubtype1) return std::unique_ptr<DeviceTypeImpl>(new Hdawg4(opts));
    return std::unique_ptr<DeviceTypeImpl>(new Hdawg());
}

// ---- SHF ----
ShfFactory::~ShfFactory() = default;                    // @ 0x2e2be0
std::unique_ptr<DeviceTypeImpl> ShfFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e2ab0 — inlines `new Shf(0)` semantics; binary calls Shf(opts) but
// makeDefault has no opts available — passes 0. Confirmed by the
// inlined `Shf::Shf(0)` which yields an empty options set.
std::unique_ptr<DeviceTypeImpl> ShfFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Shf(0ul));
}

// @ 0x2e2b00 — 8-way jump table over `(opts >> 6) & 7`. All 8 slots
// used, no default fallthrough (decoded from .rodata @ 0x962758).
std::unique_ptr<DeviceTypeImpl> ShfFactory::doMakeDeviceType(unsigned long opts) {
    switch ((opts >> 6) & 7ul) {
        case 0ul: return std::unique_ptr<DeviceTypeImpl>(new Shf(opts));
        case 1ul: return std::unique_ptr<DeviceTypeImpl>(new Shfqa2(opts));
        case 2ul: return std::unique_ptr<DeviceTypeImpl>(new Shfqa4(opts));
        case 3ul: return std::unique_ptr<DeviceTypeImpl>(new Shfsg4(opts));
        case 4ul: return std::unique_ptr<DeviceTypeImpl>(new Shfsg8(opts));
        case 5ul: return std::unique_ptr<DeviceTypeImpl>(new Shfqc(opts));
        case 6ul: return std::unique_ptr<DeviceTypeImpl>(new Shfli(opts));
        case 7ul: return std::unique_ptr<DeviceTypeImpl>(new Shfsg2(opts));
    }
    // Unreachable — `& 7` confines to 0..7 — but silence warnings.
    return std::unique_ptr<DeviceTypeImpl>(new Shf(opts));
}

// ---- SHFACC ----
ShfaccFactory::~ShfaccFactory() = default;              // @ 0x2e3530
std::unique_ptr<DeviceTypeImpl> ShfaccFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e3450 — calls Shfacc(0).
std::unique_ptr<DeviceTypeImpl> ShfaccFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Shfacc(0ul));
}

// @ 0x2e34a0 — selector mask 0x1c0; 0x80→Shfppc4, 0x40→Shfppc2,
// else→Shfacc. Note "default" subclass is Shfacc itself (not a
// distinct base).
std::unique_ptr<DeviceTypeImpl> ShfaccFactory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    if (sub == kSubtype2) return std::unique_ptr<DeviceTypeImpl>(new Shfppc4(opts));
    if (sub == kSubtype1) return std::unique_ptr<DeviceTypeImpl>(new Shfppc2(opts));
    return std::unique_ptr<DeviceTypeImpl>(new Shfacc(opts));
}

// ---- GHF ----
GhfFactory::~GhfFactory() = default;                    // @ 0x2e3920
std::unique_ptr<DeviceTypeImpl> GhfFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e3860
std::unique_ptr<DeviceTypeImpl> GhfFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Ghf(0ul));
}

// @ 0x2e38b0 — selector mask 0x1c0; 0x40→Ghfli, else→Ghf.
std::unique_ptr<DeviceTypeImpl> GhfFactory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    if (sub == kSubtype1) return std::unique_ptr<DeviceTypeImpl>(new Ghfli(opts));
    return std::unique_ptr<DeviceTypeImpl>(new Ghf(opts));
}

// ---- PQSC ----
PqscFactory::~PqscFactory() = default;                  // @ 0x2e3b40
std::unique_ptr<DeviceTypeImpl> PqscFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e3a80 — ignores opts; always Pqsc.
std::unique_ptr<DeviceTypeImpl> PqscFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Pqsc());
}

// @ 0x2e3ae0 — ignores opts; always Pqsc.
std::unique_ptr<DeviceTypeImpl> PqscFactory::doMakeDeviceType(unsigned long /*opts*/) {
    return std::unique_ptr<DeviceTypeImpl>(new Pqsc());
}

// ---- QHUB ----
QhubFactory::~QhubFactory() = default;                  // @ 0x2e3ce0
std::unique_ptr<DeviceTypeImpl> QhubFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e3c20
std::unique_ptr<DeviceTypeImpl> QhubFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Qhub());
}

// @ 0x2e3c80 — ignores opts; always Qhub.
std::unique_ptr<DeviceTypeImpl> QhubFactory::doMakeDeviceType(unsigned long /*opts*/) {
    return std::unique_ptr<DeviceTypeImpl>(new Qhub());
}

// ---- HWMOCK ----
HwmockFactory::~HwmockFactory() = default;              // @ 0x2e3f00
std::unique_ptr<DeviceTypeImpl> HwmockFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e3e60
std::unique_ptr<DeviceTypeImpl> HwmockFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Hwmock());
}

// @ 0x2e3eb0 — ignores opts; always Hwmock.
std::unique_ptr<DeviceTypeImpl> HwmockFactory::doMakeDeviceType(unsigned long /*opts*/) {
    return std::unique_ptr<DeviceTypeImpl>(new Hwmock());
}

// ---- VHF ----
VhfFactory::~VhfFactory() = default;                    // @ 0x2e41f0
std::unique_ptr<DeviceTypeImpl> VhfFactory::makeDefault() { return doMakeDefault(); }

// @ 0x2e4130
std::unique_ptr<DeviceTypeImpl> VhfFactory::doMakeDefault() {
    return std::unique_ptr<DeviceTypeImpl>(new Vhf(0ul));
}

// @ 0x2e4180 — selector mask 0x1c0; 0x40→Vhfli, else→Vhf(opts).
std::unique_ptr<DeviceTypeImpl> VhfFactory::doMakeDeviceType(unsigned long opts) {
    auto sub = opts & kSubtypeMask;
    if (sub == kSubtype1) return std::unique_ptr<DeviceTypeImpl>(new Vhfli(opts));
    return std::unique_ptr<DeviceTypeImpl>(new Vhf(opts));
}

// ===========================================================================
// makeDeviceFamilyFactory — DeviceFamily-keyed dispatcher
// @ 0x2e05d0
// ===========================================================================
std::unique_ptr<DeviceFamilyFactory>
makeDeviceFamilyFactory(DeviceFamily family) {
    switch (family) {
        case DeviceFamily::Unknown: return std::unique_ptr<DeviceFamilyFactory>(new NoDeviceTypeFactory());
        case DeviceFamily::HF2:     return std::unique_ptr<DeviceFamilyFactory>(new Hf2Factory());
        case DeviceFamily::UHF:     return std::unique_ptr<DeviceFamilyFactory>(new UhfFactory());
        case DeviceFamily::MF:      return std::unique_ptr<DeviceFamilyFactory>(new MfFactory());
        case DeviceFamily::HDAWG:   return std::unique_ptr<DeviceFamilyFactory>(new HdawgFactory());
        case DeviceFamily::SHF:     return std::unique_ptr<DeviceFamilyFactory>(new ShfFactory());
        case DeviceFamily::PQSC:    return std::unique_ptr<DeviceFamilyFactory>(new PqscFactory());
        case DeviceFamily::HWMOCK:  return std::unique_ptr<DeviceFamilyFactory>(new HwmockFactory());
        case DeviceFamily::SHFACC:  return std::unique_ptr<DeviceFamilyFactory>(new ShfaccFactory());
        case DeviceFamily::GHF:     return std::unique_ptr<DeviceFamilyFactory>(new GhfFactory());
        case DeviceFamily::QHUB:    return std::unique_ptr<DeviceFamilyFactory>(new QhubFactory());
        case DeviceFamily::VHF:     return std::unique_ptr<DeviceFamilyFactory>(new VhfFactory());
    }
    // Any DeviceFamily value not in the documented enum (e.g. the
    // synthetic 0x800 used by UnknownDevice) → catch-all.
    return std::unique_ptr<DeviceFamilyFactory>(new UnknownDeviceTypeFactory());
}

}  // namespace detail
}  // namespace zhinst
