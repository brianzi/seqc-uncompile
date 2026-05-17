// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImpl — base class + factory
// ============================================================================

#include "zhinst/asm/asm_commands_impl.hpp"
#include "zhinst/core/types.hpp"

#include <cstdint>

namespace zhinst {

AsmCommandsImpl::~AsmCommandsImpl() = default;

std::unique_ptr<AsmCommandsImpl> AsmCommandsImpl::getInstance(AwgDeviceType deviceType) {
    int val = static_cast<int>(deviceType);

    // Bit-test fast path: (devType - 2) indexes into the Hirzel-core
    // bitmap (bits {0, 6, 14, 30, 62} → devType {2, 8, 16, 32, 64}
    // = HDAWG, SHFQA, SHFSG, SHFQC_SG, SHFLI).  GHFLI and VHFLI
    // are added below since their (devType-2) doesn't fit in 64 bits.
    unsigned shifted = static_cast<unsigned>(val - 2);
    if (shifted <= 62u) {
        if ((kHirzelDevTypeMinus2Mask >> shifted) & 1u) {
            return std::make_unique<AsmCommandsImplHirzel>();
        }
    }

    // Additional Hirzel values
    if (val == 128 || val == 256) {
        return std::make_unique<AsmCommandsImplHirzel>();
    }

    // Default: Cervino
    return std::make_unique<AsmCommandsImplCervino>();
}

} // namespace zhinst
