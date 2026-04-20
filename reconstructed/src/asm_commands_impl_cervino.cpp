// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImplCervino — instruction encoding for Cervino devices
// ============================================================================

#include "zhinst/asm_commands_impl.hpp"
#include "zhinst/asm_list.hpp"
#include "zhinst/error_messages.hpp"

namespace zhinst {

AsmCommandsImplCervino::~AsmCommandsImplCervino() = default;

bool AsmCommandsImplCervino::isCWVFRSupported() const {
    return false;
}

// --- wwvfq: unsupported on Cervino ---

AsmEntry AsmCommandsImplCervino::wwvfq(int lineNumber) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::UnsupportedOp, "wwvfq"));
}

// --- wprf: opcode 0xF0000000, no registers, no immediates ---

AsmEntry AsmCommandsImplCervino::wprf(int lineNumber) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WPRF;
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = false;
    return result;
}

// --- wvf: opcode 0x20000000 ---

AsmEntry AsmCommandsImplCervino::wvf(AsmRegister waveReg, AsmRegister markerReg,
                                      int waveIndex, int lineNumber) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WVF;

    if (markerReg == AsmRegister::Reg(0)) {
        // Single-register form: waveReg in dst slot, R0 in second slot
        result.assembler.reg2 = waveReg;
        result.assembler.reg1 = AsmRegister::Reg(0);
    } else {
        // Two-register form
        result.assembler.reg2 = waveReg;
        result.assembler.reg1 = markerReg;
    }

    result.assembler.immediates.emplace_back(waveIndex);
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = isWaveformCmd(result.assembler);
    return result;
}

// --- wvfi: opcode 0x30000000 (only if markerReg == R0) ---

AsmEntry AsmCommandsImplCervino::wvfi(AsmRegister waveReg, AsmRegister markerReg,
                                       int waveIndex, int lineNumber) const {
    if (markerReg != AsmRegister::Reg(0)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvfi"));
    }

    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WVFI;
    result.assembler.reg2 = waveReg;
    result.assembler.reg1 = AsmRegister::Reg(0);
    result.assembler.immediates.emplace_back(waveIndex);
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = isWaveformCmd(result.assembler);
    return result;
}

// --- wvfs: unsupported on Cervino ---

AsmEntry AsmCommandsImplCervino::wvfs(Assembler::PlayDummyType, AsmRegister,
                                       int, int) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::UnsupportedOp, "wvfs"));
}

// --- wvft: unsupported on Cervino ---

AsmEntry AsmCommandsImplCervino::wvft(AsmRegister, int, int) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::UnsupportedOp, "wvft"));
}

// --- brz: opcode 0xF3000000 ---

AsmEntry AsmCommandsImplCervino::brz(AsmRegister reg, const std::string& label,
                                      bool flag, int lineNumber) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::BRZ;
    result.assembler.reg0 = reg;
    result.assembler.label = label;
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = flag;  // directly stored, not computed from opcode
    return result;
}

// --- ssl: opcode 0x60000005, reg in both slots ---

AsmEntry AsmCommandsImplCervino::ssl(AsmRegister reg, int lineNumber) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::SSL;
    result.assembler.reg2 = reg;  // dst
    result.assembler.reg0 = reg;  // src = same register
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = isWaveformCmd(result.assembler);
    return result;
}

// --- ssr: opcode 0x60000006, reg in both slots ---

AsmEntry AsmCommandsImplCervino::ssr(AsmRegister reg, int lineNumber) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::SSR;
    result.assembler.reg2 = reg;
    result.assembler.reg0 = reg;
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = isWaveformCmd(result.assembler);
    return result;
}

// --- ldiotrig: LD from address 0x60 ---

AsmEntry AsmCommandsImplCervino::ldiotrig(AsmRegister reg, int lineNumber) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::LD;
    result.assembler.reg0 = reg;
    result.assembler.immediates.emplace_back(0x60);
    result.wavetableFront = lineNumber;
    result.isWaveformCmd = isWaveformCmd(result.assembler);
    return result;
}

} // namespace zhinst
