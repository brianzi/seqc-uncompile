// reconstructed/src/generic_device_type.cpp
//
// detail::GenericDeviceType — Phase 14b-ii-b2.

#include "zhinst/device/generic_device_type.hpp"

#include <string>
#include <tuple>
#include <vector>

#include "zhinst/device/device_type.hpp"

namespace zhinst {
namespace detail {

// @ 0x2d3c60
//
// Source-level shape:
//
//   GenericDeviceType(string const& deviceType,
//                     vector<string> const& options)
//     : DeviceTypeImpl(std::make_tuple(
//           toDeviceTypeCode(deviceType),
//           toDeviceFamily(deviceType),
//           // Empty option set when device-type code is Unknown,
//           // mirroring the binary's `if (code != 0) ... else ...`
//           // dispatch. This avoids running toDeviceOptions() on a
//           // string that never produced a valid device type.
//           code != DeviceTypeCode::Unknown
//               ? toDeviceOptions(options, family)
//               : DeviceOptionSet(family)))
//   {
//       // ctor body: vptr override happens here implicitly via the
//       // class's vtable @ 0xb090a0 (overrides only the deleting
//       // dtor slot; doClone reuses the base impl).
//   }
//
// The disassembly inlines and reorders the tuple construction (the
// stack frame holds a temporary tuple at [rbp-0xc0..-0x70]); the
// reconstruction below mirrors the source intent rather than the
// inlined codegen.
GenericDeviceType::GenericDeviceType(
    std::string const& deviceType,
    std::vector<std::string> const& options)
    : DeviceTypeImpl(std::make_tuple(
          toDeviceTypeCode(deviceType),
          toDeviceFamily(deviceType),
          // Match the binary's two-arm dispatch: when the device-type
          // string didn't resolve to a known code, skip option parsing
          // entirely and start with an empty set carrying the family.
          (toDeviceTypeCode(deviceType) != DeviceTypeCode::Unknown
               ? toDeviceOptions(options, toDeviceFamily(deviceType))
               : DeviceOptionSet(toDeviceFamily(deviceType)))))
{
    // The compiled body computes toDeviceTypeCode/toDeviceFamily once
    // each (cached in r12d/r14d). At source level, repeating the calls
    // in the member-initializer-list above is semantically identical
    // because both functions are pure with respect to the input string.
}

// @ 0x2d4030
//
// Compiled prologue restores the base vtable into [rbx] before the
// inherited DeviceTypeImpl members are torn down. At source level, the
// implicit destructor sequence (override-vptr -> destroy members ->
// base dtor) accomplishes the same thing. The deleting variant (size
// = 0x58) is emitted by the compiler from this declaration.
GenericDeviceType::~GenericDeviceType() = default;

}  // namespace detail
}  // namespace zhinst
