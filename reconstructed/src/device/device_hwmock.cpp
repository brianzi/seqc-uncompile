// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_hwmock.cpp
// HWMOCK family (single member, default-constructed; empty options).
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

namespace zhinst {
namespace detail {

Hwmock::Hwmock() : DeviceTypeImpl(DeviceTypeCode::HWMOCK, DeviceFamily::HWMOCK) {}
Hwmock::~Hwmock() = default;
DeviceTypeImpl* Hwmock::doClone() const { return new Hwmock(*this); }

}  // namespace detail
}  // namespace zhinst
