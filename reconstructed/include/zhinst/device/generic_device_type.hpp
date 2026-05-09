// reconstructed/include/zhinst/generic_device_type.hpp
//
// detail::GenericDeviceType — the catch-all DeviceTypeImpl subclass used
// when a DeviceType is constructed from runtime strings (rather than
// from a known compile-time {family, options} factory).
//

#pragma once

#include <string>
#include <vector>

#include "zhinst/device/device_type.hpp"

namespace zhinst {
namespace detail {

// ============================================================================
// detail::GenericDeviceType — 88 bytes (0x58), same layout as DeviceTypeImpl.
//
// Adds NO data members beyond the base; only overrides the vtable so
// that clone() preserves the `GenericDeviceType` identity (vtable
// @ 0xb09038, first slot at +0x10 -> ::doClone()).
//
// The non-trivial work is entirely in the constructor: parses
// `deviceType` into a DeviceTypeCode and DeviceFamily via
// toDeviceTypeCode() and toDeviceFamily(), parses the option-name
// vector into a DeviceOptionSet via toDeviceOptions() (skipped when
// the resulting code is `Unknown` — i.e. an empty options set is used
// instead), then forwards everything as a tuple to the
// DeviceTypeImpl(tuple) base ctor.
// ============================================================================
//! \brief Catch-all `DeviceTypeImpl` subclass used when a `DeviceType`
//! is constructed from runtime device-type and option strings.
//!
//! The constructor parses the `deviceType` string into a
//! `DeviceTypeCode` and `DeviceFamily` (via `toDeviceTypeCode` /
//! `toDeviceFamily`) and the `options` string vector into a
//! `DeviceOptionSet` (via `toDeviceOptions`, family-scoped; skipped
//! when the resolved code is `Unknown`, in which case an empty
//! family-scoped option set is used) and forwards the result to the
//! `DeviceTypeImpl` base.  Adds no data members; exists so that
//! string-constructed device types remain distinguishable through
//! the polymorphic interface.
// ============================================================================
class GenericDeviceType : public DeviceTypeImpl {
public:
    // @ 0x2d3c60
    GenericDeviceType(std::string const& deviceType,
                      std::vector<std::string> const& options);

    // @ 0x2d4030
    ~GenericDeviceType() override;

    // No clone() override at the source level. The vtable for
    // GenericDeviceType (base @ 0xb09090) reuses DeviceTypeImpl::doClone
    // (0x2d3280) directly in its clone slot. Since GDT adds no fields,
    // a cloned object is a plain DeviceTypeImpl with the same code/
    // family/options — semantically a slice but functionally equivalent.
};

}  // namespace detail
}  // namespace zhinst
