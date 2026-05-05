// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_hwmock.cpp — Phase 14b-ii-b1
// HWMOCK family (single member, default-constructed; empty options).
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

namespace zhinst {
namespace detail {

Hwmock::Hwmock() : DeviceTypeImpl(DeviceTypeCode::HWMOCK, DeviceFamily::HWMOCK) {}
Hwmock::~Hwmock() = default;
DeviceTypeImpl* Hwmock::clone() const { return new Hwmock(*this); }

}  // namespace detail
}  // namespace zhinst
