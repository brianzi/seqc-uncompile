// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_shfacc.cpp
//
// SHFACC-family device subclasses: Shfacc, Shfppc2, Shfppc4.
// All three use the same inline option-bit pattern: opts & 0x20 -> FF.
// ============================================================================

#include "zhinst/device/device_subclasses.hpp"
#include "zhinst/device/device_factories.hpp"

#include <utility>

namespace zhinst {
namespace detail {

namespace {
DeviceOptionSet buildShfaccFf(unsigned long opts) {
    DeviceOptionSet set(DeviceFamily::SHFACC);
    if (opts & kDevFlagFF) set.insert(DeviceOption::FF);
    return set;
}
}  // namespace

// vtable @ .rodata 0xb09a50
Shfacc::Shfacc(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFACC, DeviceFamily::SHFACC,
                     buildShfaccFf(opts)) {}
Shfacc::~Shfacc() = default;
DeviceTypeImpl* Shfacc::doClone() const { return new Shfacc(*this); }

// vtable @ .rodata 0xb09a78
Shfppc2::Shfppc2(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFPPC2, DeviceFamily::SHFACC,
                     buildShfaccFf(opts)) {}
Shfppc2::~Shfppc2() = default;
DeviceTypeImpl* Shfppc2::doClone() const { return new Shfppc2(*this); }

// vtable @ .rodata 0xb09aa0
Shfppc4::Shfppc4(unsigned long opts)
    : DeviceTypeImpl(DeviceTypeCode::SHFPPC4, DeviceFamily::SHFACC,
                     buildShfaccFf(opts)) {}
Shfppc4::~Shfppc4() = default;
DeviceTypeImpl* Shfppc4::doClone() const { return new Shfppc4(*this); }

}  // namespace detail
}  // namespace zhinst
