// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImplHirzel — instruction encoding for Hirzel devices
// ============================================================================

#include "zhinst/asm_commands_impl.hpp"
#include "zhinst/asm_list.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/resources.hpp"  // for ResourcesException

namespace zhinst {

AsmCommandsImplHirzel::~AsmCommandsImplHirzel() = default;

bool AsmCommandsImplHirzel::isCWVFRSupported() const {
    return true;
}

// --- wwvfq: opcode 0xF0000000 (same as Cervino's wprf) ---

AsmList::Asm AsmCommandsImplHirzel::wwvfq(int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WPRF;
    result.wavetableFront = lineNumber;
    result.noOpt = false;
    return result;
}

// --- wprf: sentinel opcode 0xFFFFFFFF (no-op on Hirzel) ---

AsmList::Asm AsmCommandsImplHirzel::wprf(int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::INVALID;  // 0xFFFFFFFF
    result.wavetableFront = lineNumber;
    result.noOpt = false;
    return result;
}

// --- wvf: 0xFA000000 (single-reg) or 0x20000000 (two-reg) ---

AsmList::Asm AsmCommandsImplHirzel::wvf(AsmRegister waveReg, AsmRegister markerReg,
                                     int waveIndex, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();

    if (markerReg == AsmRegister::Reg(0)) {
        // Hirzel-specific single-register opcode
        result.assembler.cmd = Assembler::WVFE;
        result.assembler.regSrc = waveReg;
    } else {
        // Two-register form, same as Cervino
        result.assembler.cmd = Assembler::WVF;
        result.assembler.regSrc = waveReg;
        result.assembler.regAux = markerReg;
    }

    result.assembler.outputs.emplace_back(waveIndex);
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- wvfi: always throws on Hirzel ---

AsmList::Asm AsmCommandsImplHirzel::wvfi(AsmRegister, AsmRegister,
                                      int, int) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvfi"));
}

// --- wvfs: opcode 0x30000001, dummyType→bool as first immediate ---

AsmList::Asm AsmCommandsImplHirzel::wvfs(Assembler::PlayDummyType dummyType,
                                      AsmRegister reg, int arg,
                                      int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WVFS_H;

    int dummyFlag = (static_cast<int>(dummyType) != 0) ? 1 : 0;
    result.assembler.immediates.emplace_back(dummyFlag);  // child[0]: val (1-bit)
    result.assembler.regSrc = reg;                         // binary 0x27d071: reg → -0x90(%rbp) = +0x20 (regSrc)
    result.assembler.outputs.emplace_back(arg);            // child[2]: val (20-bit, after registers)

    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- wvft: opcode 0xFC000000 ---

AsmList::Asm AsmCommandsImplHirzel::wvft(AsmRegister reg, int arg,
                                      int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WVFET;
    result.assembler.regSrc = reg;  // binary 0x27d1e0: reg → +0x20 (regSrc)
    result.assembler.outputs.emplace_back(arg);  // binary 0x27d1eb: arg → +0x38 (outputs)
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- brz: reg==R0 → 0xFE000000 (unconditional); else → 0xF3000000 ---

AsmList::Asm AsmCommandsImplHirzel::brz(AsmRegister reg, const std::string& label,
                                     bool noOpt, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();

    if (reg == AsmRegister::Reg(0)) {
        result.assembler.cmd = Assembler::JMP;
        // No register set — all stay Invalid
    } else {
        result.assembler.cmd = Assembler::BRZ;
        result.assembler.regSrc = reg;
    }

    result.assembler.label = label;
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt;
    return result;
}

// --- ssl: 0x60000005, second slot = R0 (not same reg as Cervino) ---

AsmList::Asm AsmCommandsImplHirzel::ssl(AsmRegister reg, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::SSL;
    result.assembler.regDst = reg;
    result.assembler.regSrc = AsmRegister::Reg(0);  // differs from Cervino
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- ssr: 0x60000006, second slot = R0 ---

AsmList::Asm AsmCommandsImplHirzel::ssr(AsmRegister reg, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::SSR;
    result.assembler.regDst = reg;
    result.assembler.regSrc = AsmRegister::Reg(0);  // differs from Cervino
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- ldiotrig: LD from address 0x68 (Cervino uses 0x60) ---

AsmList::Asm AsmCommandsImplHirzel::ldiotrig(AsmRegister reg, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::LD;
    result.assembler.regDst = reg;
    result.assembler.outputs.emplace_back(0x68);
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

} // namespace zhinst
