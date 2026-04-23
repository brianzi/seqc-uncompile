// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_pqsc.cpp — Phase 14b-ii-b1
// PQSC family (single member, default-constructed; empty options).
// ============================================================================

#include "zhinst/device_subclasses.hpp"

namespace zhinst {
namespace detail {

Pqsc::Pqsc() : DeviceTypeImpl(DeviceTypeCode::PQSC, DeviceFamily::PQSC) {}
Pqsc::~Pqsc() = default;
DeviceTypeImpl* Pqsc::clone() const { return new Pqsc(*this); }

}  // namespace detail
}  // namespace zhinst
