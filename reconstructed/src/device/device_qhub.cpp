// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_qhub.cpp
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
