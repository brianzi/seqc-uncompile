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

    // Test bitmask 0x4000000040004041 on (val - 2).
    // Bits {0, 6, 14, 30, 62} → device values {2, 8, 16, 32, 64} → Hirzel.
    unsigned shifted = static_cast<unsigned>(val - 2);
    if (shifted <= 62u) {
        constexpr uint64_t mask = 0x4000000040004041ULL;
        if ((mask >> shifted) & 1u) {
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
