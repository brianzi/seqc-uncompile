// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommands — method implementations
// ============================================================================

#include "zhinst/asm_commands.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/resources.hpp"  // for ResourcesException
#include "zhinst/node.hpp"

#include <cstring>
#include <limits>

namespace zhinst {

// nextSequenceId() is defined in asm_list.hpp as an inline wrapper
// around AsmList::Asm::createUniqueID(false).

// =========================================================================
// Helpers
// =========================================================================

AsmList::Asm AsmCommands::emitEntry(const AssemblerInstr& instr) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = wavetableFrontIndex_;
    result.node = nullptr;
    result.isWaveformCmd = isWaveformCmd(instr);
    return result;
}

AsmList::Asm AsmCommands::emitEntry(const AssemblerInstr& instr,
                                 int overrideWavetableFront) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = overrideWavetableFront;
    result.node = nullptr;
    result.isWaveformCmd = isWaveformCmd(instr);
    return result;
}

AsmList::Asm AsmCommands::emitNodeEntry(NodeType type) const {
    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler.cmd = Assembler::INVALID;
    result.wavetableFront = wavetableFrontIndex_;
    result.node = std::make_shared<Node>();
    result.node->type = type;
    result.isWaveformCmd = false;
    return result;
}

// =========================================================================
// Waveform playback
// =========================================================================

AsmList::Asm AsmCommands::prf(AsmRegister reg1, AsmRegister reg2, int intArg) const {
    if (!isValid(reg1) || !isValid(reg2))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "prf"));

    AssemblerInstr instr;
    instr.cmd = Assembler::PRF;
    instr.reg2 = reg1;
    instr.reg0 = reg2;
    instr.immediates.emplace_back(intArg);
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::wprf() const {
    return impl_->wprf(wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::wwvfq() const {
    return impl_->wwvfq(wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::wwvf() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::WPRF;  // same opcode family as wprf
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::wvf(AsmRegister reg, AsmRegister dstReg, int length) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvf"));
    return impl_->wvf(reg, dstReg, length, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::wvfi(AsmRegister reg, AsmRegister dstReg, int length) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvfi"));
    return impl_->wvfi(reg, dstReg, length, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::wvfs(Assembler::PlayDummyType type, AsmRegister reg,
                            int length) const {
    if (static_cast<int>(type) >= 2)
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::ValueOverflow, "wvfs"));

    AsmRegister r0 = AsmRegister::Reg(0);
    AsmRegister chosen = (toInt(r0) < toInt(reg)) ? reg : r0;
    return impl_->wvfs(type, chosen, length, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::wvft(AsmRegister reg, int length) const {
    return impl_->wvft(reg, length, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::cwvf(int value) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::CWVF;
    instr.immediates.emplace_back(value);
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::cwvfr(AsmRegister reg) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::CWVFR;
    instr.reg2 = reg;
    return emitEntry(instr);
}

// =========================================================================
// Branch
// =========================================================================

AsmList::Asm AsmCommands::br(const std::string& label, bool flag) const {
    return brz(AsmRegister::Reg(0), label, flag);
}

AsmList::Asm AsmCommands::brz(AsmRegister reg, const std::string& label, bool flag) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "brz"));
    return impl_->brz(reg, label, flag, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::brnz(AsmRegister reg, const std::string& label, bool flag) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "brnz"));

    AssemblerInstr instr;
    instr.cmd = Assembler::BRNZ;
    instr.reg0 = reg;
    instr.label = label;

    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = wavetableFrontIndex_;
    result.node = nullptr;
    result.isWaveformCmd = flag;  // directly stored
    return result;
}

AsmList::Asm AsmCommands::brgz(AsmRegister reg, const std::string& label, bool flag) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "brgz"));

    AssemblerInstr instr;
    instr.cmd = Assembler::BRGZ;  // 0xF5000000, confirmed from disassembly @0x272175
    instr.reg0 = reg;
    instr.label = label;

    AsmList::Asm result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = wavetableFrontIndex_;
    result.node = nullptr;
    result.isWaveformCmd = flag;
    return result;
}

// =========================================================================
// ALU register-register
// =========================================================================

AsmList::Asm AsmCommands::alur(Assembler::Command cmd, AsmRegister dst,
                            AsmRegister src) const {
    if (!isValid(dst) || !isValid(src))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister,
                                  Assembler::commandToString(cmd).c_str()));

    AssemblerInstr instr;
    instr.cmd = cmd;
    instr.reg0 = src;
    instr.reg1 = dst;
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::addr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::ADDR, dst, src);
}

AsmList::Asm AsmCommands::subr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::SUBR, dst, src);
}

AsmList::Asm AsmCommands::andr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::ANDR, dst, src);
}

AsmList::Asm AsmCommands::orr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::ORR, dst, src);
}

AsmList::Asm AsmCommands::xnorr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::XNORR, dst, src);
}

// =========================================================================
// Shift
// =========================================================================

AsmList::Asm AsmCommands::ssl(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ssl"));
    return impl_->ssl(reg, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::ssr(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ssr"));
    return impl_->ssr(reg, wavetableFrontIndex_);
}

// =========================================================================
// ALU immediate-unsigned
// =========================================================================

AsmList::Asm AsmCommands::aluiu(Assembler::Command cmd, AsmRegister dst,
                             AsmRegister src, Immediate imm) const {
    if (!isValid(dst) || !isValid(src))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister,
                                  Assembler::commandToString(cmd).c_str()));

    AssemblerInstr instr;
    instr.cmd = cmd;
    instr.reg1 = dst;
    instr.reg0 = src;
    instr.immediates.emplace_back(imm);
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::addiu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::ADDIU, dst, src, imm);
}

AsmList::Asm AsmCommands::andiu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::ANDIU, dst, src, imm);
}

AsmList::Asm AsmCommands::oriu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::ORIU, dst, src, imm);
}

AsmList::Asm AsmCommands::xnoriu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::XNORIU, dst, src, imm);
}

// =========================================================================
// ALU immediate-signed (multi-instruction for large values)
// =========================================================================

std::vector<AsmList::Asm> AsmCommands::alui(Assembler::Command cmd, AsmRegister dst,
                                         AsmRegister src, Immediate imm) const {
    if (!isValid(dst) || !isValid(src))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister,
                                  Assembler::commandToString(cmd).c_str()));

    std::vector<AsmList::Asm> result;

    // Resolved: Immediate is a variant; expose value via operator int().
    int32_t sval = static_cast<int>(imm);
    uint32_t uval = static_cast<uint32_t>(sval);

    // Case 1: fits in ~18-bit signed range
    if (static_cast<uint32_t>(sval + 0x7FFFF) <= 0xFFFFDu) {
        AssemblerInstr instr;
        instr.cmd = cmd;
        instr.reg1 = dst;
        instr.reg0 = src;
        instr.immediates.emplace_back(sval);
        result.push_back(emitEntry(instr));
        return result;
    }

    // Case 2: ADDI with large immediate — split into low12 + upper via aluiu
    // @0x272dc0: confirmed from disassembly
    if (cmd == Assembler::ADDI) {
        // Low 12 bits via ADDI
        AssemblerInstr instr;
        instr.cmd = Assembler::ADDI;
        instr.reg1 = dst;
        instr.reg0 = src;
        instr.immediates.emplace_back(static_cast<int32_t>(uval & 0xFFF));
        result.push_back(emitEntry(instr));

        // Upper bits via single ADDIU
        uint32_t upper = uval >> 12;
        result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                               Immediate(static_cast<int32_t>(upper))));
        return result;
    }

    // Case 3: other ALU ops (ANDI, ORI, XNORI) with large immediate
    // Load constant into dst via ADDI(reg0) + optional ADDIU for upper bits,
    // then do register-register ALU operation.
    {
        AssemblerInstr loadInstr;
        loadInstr.cmd = Assembler::ADDI;
        loadInstr.reg1 = dst;
        loadInstr.reg0 = AsmRegister::Reg(0);
        loadInstr.immediates.emplace_back(static_cast<int32_t>(uval & 0xFFF));
        result.push_back(emitEntry(loadInstr));
    }

    if (uval >= 0x1000u) {
        uint32_t upper = uval >> 12;
        result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                               Immediate(static_cast<int32_t>(upper))));
    }

    // Map immediate-op to register-register equivalent:
    //   ANDI  → ANDR, ORI → ORR, XNORI → XNORR
    //   Others → throw ResourcesException
    Assembler::Command regCmd;
    if (cmd == Assembler::ANDI)
        regCmd = Assembler::ANDR;
    else if (cmd == Assembler::ORI)
        regCmd = Assembler::ORR;
    else if (cmd == Assembler::XNORI)
        regCmd = Assembler::XNORR;
    else
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT(0xd8),
                                  Assembler::commandToString(cmd).c_str()));

    // Final ALU register-register operation
    result.push_back(alur(regCmd, src, dst));
    return result;
}

std::vector<AsmList::Asm> AsmCommands::addi(AsmRegister dst, AsmRegister src,
                                         Immediate imm) const {
    return alui(Assembler::ADDI, dst, src, imm);
}

std::vector<AsmList::Asm> AsmCommands::addi32(AsmRegister dst, AsmRegister src,
                                           Immediate imm) const {
    // @0x273e30: Always splits into 2 instructions (no small-immediate fast path).
    // Emits: ADDI dst, src, (imm & 0xFFF) + ADDIU dst, dst, (imm >> 12)
    if (!isValid(dst) || !isValid(src))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "addi32"));

    std::vector<AsmList::Asm> result;
    uint32_t uval = static_cast<uint32_t>(static_cast<int>(imm));  // was: imm.value

    // Low 12 bits via ADDI
    AssemblerInstr instr;
    instr.cmd = Assembler::ADDI;
    instr.reg1 = dst;
    instr.reg0 = src;
    instr.immediates.emplace_back(static_cast<int32_t>(uval & 0xFFF));

    AsmList::Asm entry1 = emitEntry(instr);
    entry1.isWaveformCmd = true;  // forced at 0x274023

    // Upper 20 bits via ADDIU
    uint32_t upper = uval >> 12;
    AsmList::Asm entry2 = aluiu(Assembler::ADDIU, dst, dst,
                            Immediate(static_cast<int32_t>(upper)));
    entry2.isWaveformCmd = true;  // forced at 0x27402a

    result.push_back(std::move(entry1));
    result.push_back(std::move(entry2));
    return result;
}

std::vector<AsmList::Asm> AsmCommands::andi(AsmRegister dst, AsmRegister src,
                                         Immediate imm) const {
    return alui(Assembler::ANDI, dst, src, imm);
}

std::vector<AsmList::Asm> AsmCommands::ori(AsmRegister dst, AsmRegister src,
                                        Immediate imm) const {
    return alui(Assembler::ORI, dst, src, imm);
}

std::vector<AsmList::Asm> AsmCommands::xnori(AsmRegister dst, AsmRegister src,
                                          Immediate imm) const {
    return alui(Assembler::XNORI, dst, src, imm);
}

// =========================================================================
// ALU with Value — convert to int, then delegate
// =========================================================================

int AsmCommands::toInt32(Value val) const {
    // @0x279ec0: converts Value to int32, with error reporting on overflow/underflow
    try {
        return val.toInt();
    } catch (const std::overflow_error&) {
        double d = val.toDouble();
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x20), d, 32);
        if (errorHandler_) errorHandler_(msg);
        return INT_MAX;
    } catch (const std::underflow_error&) {
        double d = val.toDouble();
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x20), d, 32);
        if (errorHandler_) errorHandler_(msg);
        return INT_MIN;
    }
}

std::vector<AsmList::Asm> AsmCommands::addi(AsmRegister dst, AsmRegister src,
                                         Value val) const {
    int imm = toInt32(val);
    return addi(dst, src, Immediate(imm));
}

AsmList::Asm AsmCommands::addiu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return addiu(dst, src, Immediate(imm));
}

std::vector<AsmList::Asm> AsmCommands::andi(AsmRegister dst, AsmRegister src,
                                         Value val) const {
    int imm = toInt32(val);
    return andi(dst, src, Immediate(imm));
}

AsmList::Asm AsmCommands::andiu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return andiu(dst, src, Immediate(imm));
}

std::vector<AsmList::Asm> AsmCommands::ori(AsmRegister dst, AsmRegister src,
                                        Value val) const {
    int imm = toInt32(val);
    return ori(dst, src, Immediate(imm));
}

AsmList::Asm AsmCommands::oriu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return oriu(dst, src, Immediate(imm));
}

std::vector<AsmList::Asm> AsmCommands::xnori(AsmRegister dst, AsmRegister src,
                                          Value val) const {
    int imm = toInt32(val);
    return xnori(dst, src, Immediate(imm));
}

AsmList::Asm AsmCommands::xnoriu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return xnoriu(dst, src, Immediate(imm));
}

// =========================================================================
// Load / Store
// =========================================================================

AsmList::Asm AsmCommands::ld(AsmRegister reg,
                          detail::AddressImpl<unsigned int> addr) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ld"));

    AssemblerInstr instr;
    instr.cmd = Assembler::LD;
    instr.reg2 = reg;
    instr.immediates.emplace_back(static_cast<int32_t>(addr.value));
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::st(AsmRegister reg,
                          detail::AddressImpl<unsigned int> addr) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "st"));

    AssemblerInstr instr;
    instr.cmd = Assembler::ST;
    instr.reg0 = reg;
    instr.immediates.emplace_back(static_cast<int32_t>(addr.value));
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::ldio(AsmRegister reg, bool highBank) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ldio"));
    unsigned addr = highBank ? 0x1FEu : 0x20u;
    return ld(reg, detail::AddressImpl<unsigned int>{addr});
}

AsmList::Asm AsmCommands::sdio(AsmRegister reg, bool highBank) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "sdio"));
    unsigned addr = highBank ? 0x1FEu : 0x20u;
    return st(reg, detail::AddressImpl<unsigned int>{addr});
}

AsmList::Asm AsmCommands::luser(AsmRegister reg,
                             detail::AddressImpl<unsigned int> addr) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "luser"));
    return ld(reg, addr);  // confirmed: same opcode as ld (delegates directly @0x274b24)
}

AsmList::Asm AsmCommands::suser(AsmRegister reg,
                             detail::AddressImpl<unsigned int> addr) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "suser"));
    return st(reg, addr);  // confirmed: same opcode as st (delegates directly)
}

// =========================================================================
// Trigger
// =========================================================================

AsmList::Asm AsmCommands::ltrig(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ltrig"));
    return ld(reg, detail::AddressImpl<unsigned int>{0x22u});
}

AsmList::Asm AsmCommands::strig(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "strig"));
    return st(reg, detail::AddressImpl<unsigned int>{0x22u});
}

AsmList::Asm AsmCommands::sinttrig(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "sinttrig"));
    return st(reg, detail::AddressImpl<unsigned int>{0x23u});
}

AsmList::Asm AsmCommands::wtrig(AsmRegister r1, AsmRegister r2) const {
    if (!isValid(r1) || !isValid(r2))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wtrig"));

    AssemblerInstr instr;
    instr.cmd = Assembler::WTRIG;
    instr.reg2 = r1;
    instr.reg0 = r2;
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::wtrigi(int value) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::WTRIGI;
    instr.immediates.emplace_back(value);
    return emitEntry(instr);
}

// =========================================================================
// Misc I/O
// =========================================================================

AsmList::Asm AsmCommands::sid(AsmRegister reg, bool highBank) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "sid"));
    unsigned addr = highBank ? 0x1FFu : 0x21u;
    return st(reg, detail::AddressImpl<unsigned int>{addr});
}

// @0x275320 — 0x275620 (~0x300 bytes)
// Returns std::vector<AsmList::Asm>, NOT a single AsmList::Asm.
std::vector<AsmList::Asm> AsmCommands::smap(AsmRegister r1, AsmRegister r2, int arg) const {
    if (!isValid(r1) || !isValid(r2))                    // 0x275349, 0x27535a
        throw ResourcesException(                        // 0x275506
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "smap"));

    // Build ADDI r1, reg0, Immediate(arg) sequence       // 0x275371–0x2753e2
    std::vector<AsmList::Asm> result = alui(Assembler::ADDI, r1, AsmRegister::Reg(0), Immediate(arg));

    // Append st(r1, 0x62) and st(r2, 0x63)              // 0x275431–0x27545c
    AsmList::Asm st1 = st(r1, detail::AddressImpl<unsigned int>(0x62));   // 0x275444
    AsmList::Asm st2 = st(r2, detail::AddressImpl<unsigned int>(0x63));   // 0x27545c

    // Insert both st entries at end of result            // 0x275476
    result.push_back(std::move(st1));
    result.push_back(std::move(st2));

    return result;                                        // 0x2754f1
}

AsmList::Asm AsmCommands::ldiotrig(AsmRegister reg) const {
    return impl_->ldiotrig(reg, wavetableFrontIndex_);
}

AsmList::Asm AsmCommands::lcnt(AsmRegister reg,
                            detail::AddressImpl<unsigned int> addr) const {
    // 0x2756f0: validate register, add 0x64 (100) to address, then call ld().
    // The +0x64 offset places the address into the loop-counter register
    // bank that occupies the upper portion of the regular register address space.
    if (!reg.isValid()) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "lcnt"));
    }
    addr.value += 0x64;    // 0x27571b: add r14d, 0x64
    return ld(reg, addr);  // 0x275728
}

// =========================================================================
// Control flow / special
// =========================================================================

AsmList::Asm AsmCommands::trap() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::TRAP;
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::irpt() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::IRPT;
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::end() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::END;
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::nop() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::NOP;
    return emitEntry(instr);
}

// =========================================================================
// Sync
// =========================================================================

// 0x275c50 — syncCervino: builds a multi-instruction sync sequence.
// Returns AsmList (not AsmList::Asm). ~4288 bytes of code.
//
// The sequence sets up synchronization by writing to user registers 0x44/0x45
// with specific bit patterns and trigger sequences.
//
// flag=true path (8 instructions):
//   addi(reg2, R0, 0x400000)   — load sync mask into reg2
//   addi(reg1, R0, 1)          — load value 1 into reg1
//   suser(reg1, 0x44)          — write 1 to sync register 0x44
//   addi(reg1, R0, 0x800000)   — load trigger mask into reg1
//   wtrig(reg1, reg1)          — wait for trigger
//   suser(R0, 0x44)            — clear sync register 0x44
//   wtrig(reg2, reg2)          — wait for sync trigger
//   suser(R0, 0x44)            — clear sync register 0x44 again
//
// flag=false path (7 instructions):
//   addi(reg2, R0, 0x400000)   — load sync mask into reg2
//   addi(reg1, R0, 0x800000)   — load trigger mask into reg1
//   wtrig(reg1, reg1)          — wait for trigger
//   addi(reg1, R0, 1)          — load value 1 into reg1
//   suser(reg1, 0x45)          — write 1 to sync register 0x45
//   wtrig(reg2, reg2)          — wait for sync trigger
//   suser(R0, 0x45)            — clear sync register 0x45
AsmList AsmCommands::syncCervino(AsmRegister reg1, AsmRegister reg2,
                                  bool flag) const {
    AsmList result;

    // 0x275c81: Construct Reg(0) and Immediate(0x400000)
    // 0x275d08: alui(ADDI, reg2, Reg(0), Immediate(0x400000))
    //   — result written directly into output list
    result = alui(Assembler::ADDI, reg2, AsmRegister::Reg(0),
                  Immediate(0x400000));  // 0x275d08

    if (flag) {
        // 0x275d69: flag==true path
        // 0x275dec: alui(ADDI, reg1, Reg(0), Immediate(1))
        auto tmp = alui(Assembler::ADDI, reg1, AsmRegister::Reg(0),
                        Immediate(1));  // 0x275dec
        result.entries.insert(result.entries.end(), tmp.begin(),
                              tmp.end());  // 0x275e3d

        // 0x276062: suser(reg1, 0x44)
        result.append(suser(reg1, 0x44));  // 0x276062

        // 0x276132: alui(ADDI, reg1, Reg(0), Immediate(0x800000))
        auto tmp2 = alui(Assembler::ADDI, reg1, AsmRegister::Reg(0),
                         Immediate(0x800000));  // 0x2761a0
        result.entries.insert(result.entries.end(), tmp2.begin(),
                              tmp2.end());  // 0x2761ee

        // 0x27652c: wtrig(reg1, reg1)
        result.append(wtrig(reg1, reg1));  // 0x27653d

        // 0x2765f8: suser(Reg(0), 0x44)
        result.append(suser(AsmRegister::Reg(0), 0x44));  // 0x27661d

        // 0x2766d8: wtrig(reg2, reg2)
        result.append(wtrig(reg2, reg2));  // 0x2766ed

        // 0x2767a8: suser(Reg(0), 0x44)
        result.append(suser(AsmRegister::Reg(0), 0x44));  // 0x2767cd

    } else {
        // 0x275eb3: flag==false path
        // 0x275f36: alui(ADDI, reg1, Reg(0), Immediate(0x800000))
        auto tmp = alui(Assembler::ADDI, reg1, AsmRegister::Reg(0),
                        Immediate(0x800000));  // 0x275f36
        result.entries.insert(result.entries.end(), tmp.begin(),
                              tmp.end());  // 0x275f84

        // 0x2762c3: wtrig(reg1, reg1)
        result.append(wtrig(reg1, reg1));  // 0x2762d0

        // 0x276394: alui(ADDI, reg1, Reg(0), Immediate(1))
        auto tmp2 = alui(Assembler::ADDI, reg1, AsmRegister::Reg(0),
                         Immediate(1));  // 0x27640e
        result.entries.insert(result.entries.end(), tmp2.begin(),
                              tmp2.end());  // 0x27645f

        // 0x276857: suser(reg1, 0x45)
        result.append(suser(reg1, 0x45));  // 0x27686a

        // 0x276925: wtrig(reg2, reg2)
        result.append(wtrig(reg2, reg2));  // 0x27693a

        // 0x2769f5: suser(Reg(0), 0x45)
        result.append(suser(AsmRegister::Reg(0), 0x45));  // 0x276a1a
    }

    return result;  // 0x276ad5
}

// 0x276d10 — unsyncCervino: emits two ST instructions to clear sync registers.
// Returns AsmList (vector<AsmList::Asm>) with two entries.
// Actual return type is AsmList, not AsmList::Asm (header needs correction).
AsmList AsmCommands::unsyncCervino() const {
    // 0x276d2a: Build first AssemblerInstr — ST command
    AssemblerInstr instr1;
    instr1.cmd = Assembler::ST;                       // 0xf6000000
    instr1.reg2 = AsmRegister::Reg(0);               // 0x276dbb: dest = R0
    instr1.immediates.push_back(Immediate(0x44));    // 0x276dd3: DeviceConstants::Register::{unnamed#7} = 0x44

    // 0x276e13: Build second AssemblerInstr — ST command
    AssemblerInstr instr2;
    instr2.cmd = Assembler::ST;                       // 0xf6000000
    instr2.reg2 = AsmRegister::Reg(0);               // 0x276ea4: dest = R0
    instr2.immediates.push_back(Immediate(0x45));    // 0x276ebc: DeviceConstants::Register::{unnamed#8} = 0x45

    // 0x276efc: Initialize output AsmList (empty vector)
    AsmList result;

    // 0x276f0d: Read wavetableFrontIndex_ from this
    int wfIndex1 = wavetableFrontIndex_;              // +0x50

    // 0x276f11-0x276f20: Get TLS sequence counter, post-increment
    int seqId1 = AsmList::Asm::createUniqueID(false); // TLS+0x40

    // 0x276f2a-0x276fbf: Build first AsmList::Asm entry
    AsmList::Asm entry1;
    entry1.sequenceId = seqId1;                       // 0x276f6f
    entry1.assembler = instr1;                        // 0x276f7d: copy ctor
    entry1.wavetableFront = wfIndex1;                 // 0x276f89
    entry1.node = nullptr;                            // 0x276f97
    entry1.isWaveformCmd = false;                     // 0x276fad: (ST-3) >= 3
    result.push_back(std::move(entry1));

    // 0x277019: Read wavetableFrontIndex_ again
    int wfIndex2 = wavetableFrontIndex_;              // +0x50

    // 0x27701d-0x277023: Get TLS sequence counter again, post-increment
    int seqId2 = AsmList::Asm::createUniqueID(false); // TLS+0x40

    // 0x277026-0x2770b2: Build second AsmList::Asm entry
    AsmList::Asm entry2;
    entry2.sequenceId = seqId2;                       // 0x277067
    entry2.assembler = instr2;                        // 0x277074: copy ctor
    entry2.wavetableFront = wfIndex2;                 // 0x27707c
    entry2.node = nullptr;                            // 0x27708a
    entry2.isWaveformCmd = false;                     // 0x2770a0: (ST-3) >= 3
    result.push_back(std::move(entry2));

    // 0x277103-0x27711e: Destroy local AssemblerInstrs
    return result;                                    // 0x277123
}

AsmList::Asm AsmCommands::asmSyncPlaceholderCervino() const {
    return emitNodeEntry(NodeType::SyncPlaceholderCervino);
}

AsmList::Asm AsmCommands::asmSyncHirzel() const {
    return suser(AsmRegister::Reg(0), detail::AddressImpl<unsigned int>{0x6Eu});
}

// =========================================================================
// Pseudo-instructions / directives
// =========================================================================

AsmList::Asm AsmCommands::asmZero(AsmRegister reg) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::ADDI;
    instr.reg2 = reg;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(0);
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::asmOne(AsmRegister reg) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::ADDI;
    instr.reg2 = reg;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(1);
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::asmLabel(const std::string& label) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::LABEL;
    instr.label = label;
    return emitEntry(instr, 0);  // wavetableFront forced to 0
}

AsmList::Asm AsmCommands::asmMessage(const std::string& msg, bool isError) const {
    // @0x277630: Command is 5 (error) or 3 (message). String stored as Immediate.
    AssemblerInstr instr;
    instr.cmd = isError ? Assembler::Command(5) : Assembler::Command(3);
    instr.immediates.emplace_back(Immediate(msg));
    return emitEntry(instr, 0);  // wavetableFront forced to 0
}

// =========================================================================
// Node-creating commands
// =========================================================================

AsmList::Asm AsmCommands::asmBranchNode() const {
    return emitNodeEntry(NodeType::Branch);
}

AsmList::Asm AsmCommands::asmLoopNode() const {
    return emitNodeEntry(NodeType::Loop);
}

AsmList::Asm AsmCommands::asmRate(int rate) const {
    AsmList::Asm result = emitNodeEntry(NodeType::Rate);
    result.node->globalRate = rate;  // +0x100
    return result;
}

AsmList::Asm AsmCommands::asmSetPrecompFlags(unsigned int flags) const {
    AsmList::Asm result = emitNodeEntry(NodeType::PrecompFlags);
    result.node->defaultPrecompFlags = flags;  // +0x104
    return result;
}

// =========================================================================
// Placeholder / waveform management
// =========================================================================

AsmList::Asm AsmCommands::asmSetVarPlaceholder(AsmRegister reg) {
    AsmList::Asm result = emitNodeEntry(NodeType::SetVarPlaceholder);
    result.node->lengthReg = reg;  // +0x88
    return result;
}

AsmList::Asm AsmCommands::asmLockPlaceholder(std::shared_ptr<WaveformFront> wvf,
                                          int index) {
    AsmList::Asm result = emitNodeEntry(NodeType::LockPlaceholder);
    // Waveform name is at offset 0x00 of WaveformFront (direct field access, confirmed)
    result.node->wavesPerDev[index] = wvf->name;
    result.node->deviceIndex = index;
    return result;
}

AsmList::Asm AsmCommands::asmUnlockPlaceholder(std::shared_ptr<WaveformFront> wvf,
                                             int index) {
    AsmList::Asm result = emitNodeEntry(NodeType::UnlockPlaceholder);
    // Copy waveform name into node->wavesPerDev[index]
    result.node->deviceIndex = index;
    return result;
}

AsmList::Asm AsmCommands::asmLoadPlaceholder() {
    return emitNodeEntry(NodeType::Load);
}

AsmList::Asm AsmCommands::asmPrefetch(std::shared_ptr<WaveformFront> wvf,
                                   int nameIndex, int regVal, int extraVal) {
    AsmList::Asm result = emitNodeEntry(NodeType::Prefetch);
    result.node->lengthReg = static_cast<AsmRegister>(regVal);
    result.node->length = extraVal;

    if (wvf) {
        wvf->used = true;
        // Copy waveform name into node->wavesPerDev[nameIndex]
    }

    result.node->deviceIndex = nameIndex;
    return result;
}

// =========================================================================
// Play / Table
// =========================================================================

PlayConfig AsmCommands::genPlayConfig(
    const std::shared_ptr<WaveformFront>& wvf,
    bool isHold, bool fourChannel, bool isFourChannelBool,
    bool isBool, int holdCount, unsigned int suppress,
    bool isHoldMode, unsigned int trigger) const
{
    // Disassembly: 0x2789a0
    // Field references verified from binary:
    //   [wf+0xc8]      = signal.channels_     (uint16_t, NOT "sampleWidth")
    //   [wf+0xb0/+0xb8] = signal.markerBits_  vector begin/end
    // Parameters `holdCount` and `isBool` are accepted but never consumed by
    // the binary in the visible code path; included here for ABI fidelity.
    PlayConfig config;
    (void)holdCount;
    (void)isBool;
    config.suppress = suppress;
    config.is4Channel = isFourChannelBool;
    config.trigger = trigger;
    config.precompFlags = 0;
    config.hold = isHoldMode;

    WaveformFront* wf = wvf.get();
    if (!wf) {
        config.channelMask = 0;
        config.markerBits = 0;
        config.dummy = true;
        return config;
    }

    config.dummy = false;
    // 0x2789d7: movzx ecx,WORD PTR [rax+0xc8] = signal.channels_
    uint16_t channels = wf->signal.channels_;
    uint32_t fullMask = (1u << channels) - 1;       // 0x2789de..0x2789e7
    uint32_t channelMask = (channels == 1) ? 2u : fullMask; // 0x2789eb..0x2789f1
    if (!isHold) channelMask = fullMask;             // 0x2789f5..0x2789f8 (cmove on r15b)
    config.channelMask = channelMask;

    // Marker processing: iterate signal.markerBits_ pairwise from end.
    // 0x278a0c..0x278a1e: load begin/end pointers, compute count
    auto& mbits = wf->signal.markerBits_;
    const uint8_t* data = mbits.data();
    uint16_t count = static_cast<uint16_t>(mbits.size());

    uint32_t markerBits = 0;
    if (count >= 2) {
        unsigned pairs = trigger & 0xFFFE;
        for (unsigned i = 0; i < pairs; i += 2) {
            uint8_t b1 = data[count - 1];
            uint8_t v1 = (b1 | (b1 >> 1)) & 0x3;
            markerBits = (markerBits << 4) | (v1 << 2);
            uint8_t b0 = data[count - 2];
            uint8_t v0 = (b0 | (b0 >> 1)) & 0x3;
            markerBits |= v0;
            count -= 2;
        }
    }

    if (trigger & 1) {
        uint8_t b = data[count - 1];
        uint8_t v = (b | (b >> 1)) & 0x3;
        markerBits = (markerBits << 2) | v;
    }

    uint32_t adjusted = markerBits << 2;
    if (markerBits >= 4) adjusted = markerBits;
    if (!isHold) adjusted = markerBits;
    config.markerBits = adjusted;

    return config;
}

AsmList::Asm AsmCommands::asmPlay(
    std::vector<std::shared_ptr<WaveformFront>> waveforms,
    int nameIndex, bool isHold, bool fourChannel, bool isBool,
    int holdCount, unsigned int suppress, bool isHoldMode,
    AsmRegister reg, int regVal, AsmRegister reg2,
    unsigned int trigger)
{
    AsmList::Asm result = emitNodeEntry(NodeType::Play);
    Node* node = result.node.get();

    node->deviceIndex = nameIndex;

    // Build waveform name vector
    for (auto& wvf : waveforms) {
        WaveformFront* wf = wvf.get();
        if (wf) {
            // Waveform name at offset 0x00 of WaveformFront (direct field access)
            node->wavesPerDev.push_back(wf->name);
        } else {
            node->wavesPerDev.push_back(std::nullopt);
        }
    }

    // Compute play config
    std::shared_ptr<WaveformFront> currentWvf;
    if (nameIndex >= 0 && nameIndex < static_cast<int>(waveforms.size())) {
        currentWvf = waveforms[nameIndex];
    }

    node->config = genPlayConfig(currentWvf, isHold, fourChannel,
                                       fourChannel, isBool, holdCount,
                                       suppress, isHoldMode, trigger);

    // Set register info
    node->lengthReg = reg;
    node->indexOffsetReg = reg2;
    node->length = regVal;

    // Mark waveform as used.
    //
    // NOTE: A previous reconstruction conditionally called
    //   currentWvf->playWord = node->config.encodeCwvf(defaultRate);
    // here. That was a hallucination. Verified against the disassembly of
    // asmPlay at 0x278b40: PlayConfig::encodeCwvf (0x1dc500) is called from
    // exactly one site in the binary — 0x1d8075, inside
    // Prefetch::placeSingleCommand (0x1d7940) — and never from asmPlay.
    if (currentWvf) {
        currentWvf->used = true;
    }

    return result;
}

AsmList::Asm AsmCommands::asmTable(int tableIndex, std::shared_ptr<WaveformFront> wvf,
                                int nameIndex, bool isHold, bool fourChannel,
                                int holdCount, unsigned int suppress,
                                bool isHoldMode, AsmRegister reg, int regVal) {
    AsmList::Asm result = emitNodeEntry(NodeType::Table);
    Node* node = result.node.get();

    node->config = genPlayConfig(wvf, isHold, fourChannel, fourChannel,
                                       isHoldMode, holdCount, suppress,
                                       false, 0);

    node->lengthReg = reg;
    node->length = regVal;

    if (wvf) {
        wvf->used = true;
        // Copy waveform name into node->wavesPerDev[nameIndex]
    }

    node->deviceIndex = nameIndex;
    node->tableIndex = tableIndex;

    return result;
}

// =========================================================================
// Misc
// =========================================================================

AsmList::Asm AsmCommands::asmWtrigLSPlaceholder(int value) {
    AssemblerInstr instr;
    instr.cmd = Assembler::ST;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(value + 0x40);
    return emitEntry(instr);
}

AsmList::Asm AsmCommands::fb(int value) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::FB;
    instr.immediates.emplace_back(value);
    return emitEntry(instr);
}

} // namespace zhinst
