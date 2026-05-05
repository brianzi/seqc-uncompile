// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_qhub.cpp — Phase 14b-ii-b1
// QHUB family (single member, default-constructed; empty options).
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

namespace zhinst {
namespace detail {

Qhub::Qhub() : DeviceTypeImpl(DeviceTypeCode::QHUB, DeviceFamily::QHUB) {}
Qhub::~Qhub() = default;
DeviceTypeImpl* Qhub::clone() const { return new Qhub(*this); }

}  // namespace detail
}  // namespace zhinst
