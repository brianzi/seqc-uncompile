// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_pqsc.cpp
// PQSC family (single member, default-constructed; empty options).
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"

namespace zhinst {
namespace detail {

Pqsc::Pqsc() : DeviceTypeImpl(DeviceTypeCode::PQSC, DeviceFamily::PQSC) {}
Pqsc::~Pqsc() = default;
DeviceTypeImpl* Pqsc::doClone() const { return new Pqsc(*this); }

}  // namespace detail
}  // namespace zhinst
