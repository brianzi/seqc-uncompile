// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_unknown.cpp
//
// detail::UnknownDevice — a special subclass produced by the
// DeviceType(string, ...) parser when the device-type string isn't
// recognised, and by UnknownDeviceTypeFactory.
//
// The binary stores synthetic sentinel values:
//   code   = 33     (DeviceTypeCode::Unknown is 0; 33 is OUTSIDE the
//                    documented 0..32 range and is what
//                    toDeviceTypeCode() returns for an unrecognised
//                    string. toString(DeviceTypeCode(33)) returns
//                    "unknown".)
//   family = 0x800  (NOT a documented DeviceFamily enum value — sits
//                    one bit above VHF=0x400 in the one-hot bitfield.
//                    toString(DeviceFamily(0x800)) returns "unknown".)
//
// Source: ctor at 0x2d3320 has the line
//   movabs rax, 0x80000000021
//   mov    [rdi+0x8], rax
// which writes both fields together (low 32 = code, high 32 = family).
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

namespace zhinst {
namespace detail {

// @ 0x2d3320 — vtable @ .rodata 0xb09038.
//
// The binary inlines the body rather than calling the
// DeviceTypeImpl(code, family) ctor: it sets the base vtable, writes
// code/family directly via a 64-bit move, then constructs only the
// DeviceOptionSet member. The sequence is observably equivalent to
// invoking the (code, family) base ctor (which itself does the same
// three writes plus the redundant base-vtable set that gets
// immediately overridden by the derived vtable).
UnknownDevice::UnknownDevice()
    : DeviceTypeImpl(static_cast<DeviceTypeCode>(33),
                     static_cast<DeviceFamily>(0x800)) {}

UnknownDevice::~UnknownDevice() = default;

DeviceTypeImpl* UnknownDevice::doClone() const {
    return new UnknownDevice(*this);
}

}  // namespace detail
}  // namespace zhinst
