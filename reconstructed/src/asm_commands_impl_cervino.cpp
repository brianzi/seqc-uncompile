// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImplCervino — instruction encoding for Cervino devices
// ============================================================================

#include "zhinst/asm_commands_impl.hpp"
#include "zhinst/asm_list.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/resources.hpp"  // for ResourcesException

namespace zhinst {

AsmCommandsImplCervino::~AsmCommandsImplCervino() = default;

bool AsmCommandsImplCervino::isCWVFRSupported() const {
    return false;
}

// --- wwvfq: unsupported on Cervino ---

AsmList::Asm AsmCommandsImplCervino::wwvfq(int lineNumber) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::UnsupportedOp, "wwvfq"));
}

// --- wprf: opcode 0xF0000000, no registers, no immediates ---

AsmList::Asm AsmCommandsImplCervino::wprf(int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WPRF;
    result.wavetableFront = lineNumber;
    result.noOpt = false;
    return result;
}

// --- wvf: opcode 0x20000000 ---

AsmList::Asm AsmCommandsImplCervino::wvf(AsmRegister waveReg, AsmRegister dstReg,
                                      int length, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WVF;

    if (dstReg == AsmRegister::Reg(0)) {
        // Single-register form: waveReg in aux slot (bits 27:24), R0 in src slot (bits 23:20)
        result.assembler.regAux = waveReg;
        result.assembler.regSrc = AsmRegister::Reg(0);
    } else {
        // Two-register form
        result.assembler.regAux = waveReg;
        result.assembler.regSrc = dstReg;
    }

    // length goes in outputs (not immediates) so it appears AFTER registers
    // in the child ordering: immediates → regDst → regAux → regSrc → outputs
    // opcode3 expects children: [reg(child[0]), reg(child[1]), val(child[2])]
    result.assembler.outputs.emplace_back(length);
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- wvfi: opcode 0x30000000 (only if dstReg == R0) ---

AsmList::Asm AsmCommandsImplCervino::wvfi(AsmRegister waveReg, AsmRegister dstReg,
                                       int length, int lineNumber) const {
    if (dstReg != AsmRegister::Reg(0)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvfi"));
    }

    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::WVFI;
    result.assembler.regAux = waveReg;
    result.assembler.regSrc = AsmRegister::Reg(0);
    result.assembler.outputs.emplace_back(length);  // outputs, not immediates (child order)
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- wvfs: unsupported on Cervino ---

AsmList::Asm AsmCommandsImplCervino::wvfs(Assembler::PlayDummyType, AsmRegister,
                                       int, int) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::UnsupportedOp, "wvfs"));
}

// --- wvft: unsupported on Cervino ---

AsmList::Asm AsmCommandsImplCervino::wvft(AsmRegister, int, int) const {
    throw ResourcesException(
        ErrorMessages::format(ErrorMessageT::UnsupportedOp, "wvft"));
}

// --- brz: opcode 0xF3000000 ---

AsmList::Asm AsmCommandsImplCervino::brz(AsmRegister reg, const std::string& label,
                                      bool noOpt, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::BRZ;
    result.assembler.regSrc = reg;
    result.assembler.label = label;
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt;  // directly stored, not computed from opcode
    return result;
}

// --- ssl: opcode 0x60000005, reg in both slots ---

AsmList::Asm AsmCommandsImplCervino::ssl(AsmRegister reg, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::SSL;
    result.assembler.regSrc = reg;  // dst
    result.assembler.regDst = reg;  // src = same register
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- ssr: opcode 0x60000006, reg in both slots ---

AsmList::Asm AsmCommandsImplCervino::ssr(AsmRegister reg, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::SSR;
    result.assembler.regSrc = reg;
    result.assembler.regDst = reg;
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

// --- ldiotrig: LD from address 0x60 ---

AsmList::Asm AsmCommandsImplCervino::ldiotrig(AsmRegister reg, int lineNumber) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::LD;
    result.assembler.regDst = reg;
    result.assembler.outputs.emplace_back(0x60);
    result.wavetableFront = lineNumber;
    result.noOpt = noOpt(result.assembler);
    return result;
}

} // namespace zhinst
