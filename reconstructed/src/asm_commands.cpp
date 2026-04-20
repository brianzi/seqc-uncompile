// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommands — method implementations
// ============================================================================

#include "zhinst/asm_commands.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/node.hpp"

#include <cstring>
#include <limits>

namespace zhinst {

// Thread-local sequence counter (TLS offset 0x40 in original binary)
static thread_local int s_sequenceCounter = 0;

int nextSequenceId() {
    return s_sequenceCounter++;
}

// =========================================================================
// Helpers
// =========================================================================

AsmEntry AsmCommands::emitEntry(const AssemblerInstr& instr) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = wavetableFrontIndex_;
    result.node = nullptr;
    result.isWaveformCmd = isWaveformCmd(instr);
    return result;
}

AsmEntry AsmCommands::emitEntry(const AssemblerInstr& instr,
                                 int overrideWavetableFront) const {
    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = overrideWavetableFront;
    result.node = nullptr;
    result.isWaveformCmd = isWaveformCmd(instr);
    return result;
}

AsmEntry AsmCommands::emitNodeEntry(NodeType type) const {
    AsmEntry result;
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

AsmEntry AsmCommands::prf(AsmRegister reg1, AsmRegister reg2, int intArg) const {
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

AsmEntry AsmCommands::wprf() const {
    return impl_->wprf(wavetableFrontIndex_);
}

AsmEntry AsmCommands::wwvfq() const {
    return impl_->wwvfq(wavetableFrontIndex_);
}

AsmEntry AsmCommands::wwvf() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::WPRF;  // same opcode family as wprf
    return emitEntry(instr);
}

AsmEntry AsmCommands::wvf(AsmRegister reg, AsmRegister dstReg, int length) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvf"));
    return impl_->wvf(reg, dstReg, length, wavetableFrontIndex_);
}

AsmEntry AsmCommands::wvfi(AsmRegister reg, AsmRegister dstReg, int length) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wvfi"));
    return impl_->wvfi(reg, dstReg, length, wavetableFrontIndex_);
}

AsmEntry AsmCommands::wvfs(Assembler::PlayDummyType type, AsmRegister reg,
                            int length) const {
    if (static_cast<int>(type) >= 2)
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::ValueOverflow, "wvfs"));

    AsmRegister r0 = AsmRegister::Reg(0);
    AsmRegister chosen = (toInt(r0) < toInt(reg)) ? reg : r0;
    return impl_->wvfs(type, chosen, length, wavetableFrontIndex_);
}

AsmEntry AsmCommands::wvft(AsmRegister reg, int length) const {
    return impl_->wvft(reg, length, wavetableFrontIndex_);
}

AsmEntry AsmCommands::cwvf(int value) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::CWVF;
    instr.immediates.emplace_back(value);
    return emitEntry(instr);
}

AsmEntry AsmCommands::cwvfr(AsmRegister reg) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::CWVFR;
    instr.reg2 = reg;
    return emitEntry(instr);
}

// =========================================================================
// Branch
// =========================================================================

AsmEntry AsmCommands::br(const std::string& label, bool flag) const {
    return brz(AsmRegister::Reg(0), label, flag);
}

AsmEntry AsmCommands::brz(AsmRegister reg, const std::string& label, bool flag) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "brz"));
    return impl_->brz(reg, label, flag, wavetableFrontIndex_);
}

AsmEntry AsmCommands::brnz(AsmRegister reg, const std::string& label, bool flag) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "brnz"));

    AssemblerInstr instr;
    instr.cmd = Assembler::BRNZ;
    instr.reg0 = reg;
    instr.label = label;

    AsmEntry result;
    result.sequenceId = nextSequenceId();
    result.assembler = instr;
    result.wavetableFront = wavetableFrontIndex_;
    result.node = nullptr;
    result.isWaveformCmd = flag;  // directly stored
    return result;
}

AsmEntry AsmCommands::brgz(AsmRegister reg, const std::string& label, bool flag) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "brgz"));

    AssemblerInstr instr;
    instr.cmd = Assembler::BRNZ;  // TODO: may be a different opcode
    instr.reg0 = reg;
    instr.label = label;

    AsmEntry result;
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

AsmEntry AsmCommands::alur(Assembler::Command cmd, AsmRegister dst,
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

AsmEntry AsmCommands::addr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::ADDR, dst, src);
}

AsmEntry AsmCommands::subr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::SUBR, dst, src);
}

AsmEntry AsmCommands::andr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::ANDR, dst, src);
}

AsmEntry AsmCommands::orr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::ORR, dst, src);
}

AsmEntry AsmCommands::xnorr(AsmRegister dst, AsmRegister src) const {
    return alur(Assembler::XNORR, dst, src);
}

// =========================================================================
// Shift
// =========================================================================

AsmEntry AsmCommands::ssl(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ssl"));
    return impl_->ssl(reg, wavetableFrontIndex_);
}

AsmEntry AsmCommands::ssr(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ssr"));
    return impl_->ssr(reg, wavetableFrontIndex_);
}

// =========================================================================
// ALU immediate-unsigned
// =========================================================================

AsmEntry AsmCommands::aluiu(Assembler::Command cmd, AsmRegister dst,
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

AsmEntry AsmCommands::addiu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::ADDIU, dst, src, imm);
}

AsmEntry AsmCommands::andiu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::ANDIU, dst, src, imm);
}

AsmEntry AsmCommands::oriu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::ORIU, dst, src, imm);
}

AsmEntry AsmCommands::xnoriu(AsmRegister dst, AsmRegister src, Immediate imm) const {
    return aluiu(Assembler::XNORIU, dst, src, imm);
}

// =========================================================================
// ALU immediate-signed (multi-instruction for large values)
// =========================================================================

std::vector<AsmEntry> AsmCommands::alui(Assembler::Command cmd, AsmRegister dst,
                                         AsmRegister src, Immediate imm) const {
    if (!isValid(dst) || !isValid(src))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister,
                                  Assembler::commandToString(cmd).c_str()));

    std::vector<AsmEntry> result;

    int32_t sval = imm.value;
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

    // Case 2: ADDI with large immediate — split into low + upper
    if (cmd == Assembler::ADDI) {
        // Low 12 bits via ADDI
        AssemblerInstr instr;
        instr.cmd = Assembler::ADDI;
        instr.reg1 = dst;
        instr.reg0 = src;
        instr.immediates.emplace_back(static_cast<int32_t>(uval & 0xFFF));
        result.push_back(emitEntry(instr));

        // Upper bits via ADDIU chain
        uint32_t upper = uval >> 12;
        if (upper >= 0x1000u) {
            result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                                   Immediate(static_cast<int32_t>(upper & 0xFFF))));
            upper >>= 12;
            if (upper > 0) {
                result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                                       Immediate(static_cast<int32_t>(upper))));
            }
        } else {
            result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                                   Immediate(static_cast<int32_t>(upper))));
        }
        return result;
    }

    // Case 3: other ALU ops with large immediate
    // Load constant into dst via ADDI+ADDIU chain, then do reg-reg ALU
    {
        AssemblerInstr loadInstr;
        loadInstr.cmd = Assembler::ADDI;
        loadInstr.reg1 = dst;
        loadInstr.reg0 = AsmRegister::Reg(0);
        loadInstr.immediates.emplace_back(static_cast<int32_t>(uval & 0xFFF));
        result.push_back(emitEntry(loadInstr));
    }

    uint32_t upper = uval >> 12;
    result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                           Immediate(static_cast<int32_t>(upper & 0xFFF))));

    if (upper >= 0x1000u) {
        upper >>= 12;
        result.push_back(aluiu(Assembler::ADDIU, dst, dst,
                               Immediate(static_cast<int32_t>(upper))));
    }

    // Final ALU register-register operation
    result.push_back(alur(cmd, dst, src));
    return result;
}

std::vector<AsmEntry> AsmCommands::addi(AsmRegister dst, AsmRegister src,
                                         Immediate imm) const {
    return alui(Assembler::ADDI, dst, src, imm);
}

std::vector<AsmEntry> AsmCommands::addi32(AsmRegister dst, AsmRegister src,
                                           Immediate imm) const {
    // Full 32-bit variant — handles full range directly.
    // TODO: reconstruct exact logic (complex multi-instruction emission)
    return alui(Assembler::ADDI, dst, src, imm);
}

std::vector<AsmEntry> AsmCommands::andi(AsmRegister dst, AsmRegister src,
                                         Immediate imm) const {
    return alui(Assembler::ANDI, dst, src, imm);
}

std::vector<AsmEntry> AsmCommands::ori(AsmRegister dst, AsmRegister src,
                                        Immediate imm) const {
    return alui(Assembler::ORI, dst, src, imm);
}

std::vector<AsmEntry> AsmCommands::xnori(AsmRegister dst, AsmRegister src,
                                          Immediate imm) const {
    return alui(Assembler::XNORI, dst, src, imm);
}

// =========================================================================
// ALU with Value — convert to int, then delegate
// =========================================================================

int AsmCommands::toInt32(Value val) const {
    // TODO: proper implementation with overflow/underflow handling
    // Original catches overflow_error → returns INT_MAX with error report
    // Original catches underflow_error → returns INT_MIN with error report
    (void)val;
    return 0;  // placeholder
}

std::vector<AsmEntry> AsmCommands::addi(AsmRegister dst, AsmRegister src,
                                         Value val) const {
    int imm = toInt32(val);
    return addi(dst, src, Immediate(imm));
}

AsmEntry AsmCommands::addiu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return addiu(dst, src, Immediate(imm));
}

std::vector<AsmEntry> AsmCommands::andi(AsmRegister dst, AsmRegister src,
                                         Value val) const {
    int imm = toInt32(val);
    return andi(dst, src, Immediate(imm));
}

AsmEntry AsmCommands::andiu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return andiu(dst, src, Immediate(imm));
}

std::vector<AsmEntry> AsmCommands::ori(AsmRegister dst, AsmRegister src,
                                        Value val) const {
    int imm = toInt32(val);
    return ori(dst, src, Immediate(imm));
}

AsmEntry AsmCommands::oriu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return oriu(dst, src, Immediate(imm));
}

std::vector<AsmEntry> AsmCommands::xnori(AsmRegister dst, AsmRegister src,
                                          Value val) const {
    int imm = toInt32(val);
    return xnori(dst, src, Immediate(imm));
}

AsmEntry AsmCommands::xnoriu(AsmRegister dst, AsmRegister src, Value val) const {
    int imm = toInt32(val);
    return xnoriu(dst, src, Immediate(imm));
}

// =========================================================================
// Load / Store
// =========================================================================

AsmEntry AsmCommands::ld(AsmRegister reg,
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

AsmEntry AsmCommands::st(AsmRegister reg,
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

AsmEntry AsmCommands::ldio(AsmRegister reg, bool highBank) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ldio"));
    unsigned addr = highBank ? 0x1FEu : 0x20u;
    return ld(reg, detail::AddressImpl<unsigned int>{addr});
}

AsmEntry AsmCommands::sdio(AsmRegister reg, bool highBank) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "sdio"));
    unsigned addr = highBank ? 0x1FEu : 0x20u;
    return st(reg, detail::AddressImpl<unsigned int>{addr});
}

AsmEntry AsmCommands::luser(AsmRegister reg,
                             detail::AddressImpl<unsigned int> addr) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "luser"));
    return ld(reg, addr);  // TODO: may use different opcode
}

AsmEntry AsmCommands::suser(AsmRegister reg,
                             detail::AddressImpl<unsigned int> addr) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "suser"));
    return st(reg, addr);  // TODO: may use different opcode
}

// =========================================================================
// Trigger
// =========================================================================

AsmEntry AsmCommands::ltrig(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "ltrig"));
    return ld(reg, detail::AddressImpl<unsigned int>{0x22u});
}

AsmEntry AsmCommands::strig(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "strig"));
    return st(reg, detail::AddressImpl<unsigned int>{0x22u});
}

AsmEntry AsmCommands::sinttrig(AsmRegister reg) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "sinttrig"));
    return st(reg, detail::AddressImpl<unsigned int>{0x23u});
}

AsmEntry AsmCommands::wtrig(AsmRegister r1, AsmRegister r2) const {
    if (!isValid(r1) || !isValid(r2))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "wtrig"));

    AssemblerInstr instr;
    instr.cmd = Assembler::WTRIG;
    instr.reg2 = r1;
    instr.reg0 = r2;
    return emitEntry(instr);
}

AsmEntry AsmCommands::wtrigi(int value) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::WTRIGI;
    instr.immediates.emplace_back(value);
    return emitEntry(instr);
}

// =========================================================================
// Misc I/O
// =========================================================================

AsmEntry AsmCommands::sid(AsmRegister reg, bool highBank) const {
    if (!isValid(reg))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "sid"));
    unsigned addr = highBank ? 0x1FFu : 0x21u;
    return st(reg, detail::AddressImpl<unsigned int>{addr});
}

AsmEntry AsmCommands::smap(AsmRegister r1, AsmRegister r2, int arg) const {
    if (!isValid(r1) || !isValid(r2))
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::InvalidRegister, "smap"));
    // Uses ADDI to compute, then maps
    // TODO: verify exact implementation
    AssemblerInstr instr;
    instr.cmd = Assembler::ADDI;
    instr.reg1 = r1;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(arg);
    return emitEntry(instr);
}

AsmEntry AsmCommands::ldiotrig(AsmRegister reg) const {
    return impl_->ldiotrig(reg, wavetableFrontIndex_);
}

AsmEntry AsmCommands::lcnt(AsmRegister reg,
                            detail::AddressImpl<unsigned int> addr) const {
    // Similar to ld — exact opcode TBD
    return ld(reg, addr);
}

// =========================================================================
// Control flow / special
// =========================================================================

AsmEntry AsmCommands::trap() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::TRAP;
    return emitEntry(instr);
}

AsmEntry AsmCommands::irpt() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::IRPT;
    return emitEntry(instr);
}

AsmEntry AsmCommands::end() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::END;
    return emitEntry(instr);
}

AsmEntry AsmCommands::nop() const {
    AssemblerInstr instr;
    instr.cmd = Assembler::NOP;
    return emitEntry(instr);
}

// =========================================================================
// Sync
// =========================================================================

AsmEntry AsmCommands::syncCervino(AsmRegister reg1, AsmRegister reg2,
                                   bool flag) const {
    // Complex multi-instruction sync sequence (~1000 lines of asm).
    // Builds Immediate(0x400000), calls addiu, then further setup.
    // TODO: full reconstruction from disassembly
    (void)reg1; (void)reg2; (void)flag;
    return emitNodeEntry(NodeType::SyncPlaceholderCervino);
}

AsmEntry AsmCommands::unsyncCervino() const {
    // Complex multi-instruction unsync sequence.
    // TODO: full reconstruction from disassembly
    return emitNodeEntry(NodeType::SyncPlaceholderCervino);
}

AsmEntry AsmCommands::asmSyncPlaceholderCervino() const {
    return emitNodeEntry(NodeType::SyncPlaceholderCervino);
}

AsmEntry AsmCommands::asmSyncHirzel() const {
    return suser(AsmRegister::Reg(0), detail::AddressImpl<unsigned int>{0x6Eu});
}

// =========================================================================
// Pseudo-instructions / directives
// =========================================================================

AsmEntry AsmCommands::asmZero(AsmRegister reg) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::ADDI;
    instr.reg2 = reg;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(0);
    return emitEntry(instr);
}

AsmEntry AsmCommands::asmOne(AsmRegister reg) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::ADDI;
    instr.reg2 = reg;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(1);
    return emitEntry(instr);
}

AsmEntry AsmCommands::asmLabel(const std::string& label) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::LABEL;
    instr.label = label;
    return emitEntry(instr, 0);  // wavetableFront forced to 0
}

AsmEntry AsmCommands::asmMessage(const std::string& msg, bool isError) const {
    AssemblerInstr instr;
    instr.cmd = isError ? Assembler::ERROR_MSG : Assembler::MESSAGE;
    instr.immediates.emplace_back(0);  // placeholder for string immediate
    // TODO: message string is stored as an Immediate — need Immediate(string)
    return emitEntry(instr, 0);  // wavetableFront forced to 0
}

// =========================================================================
// Node-creating commands
// =========================================================================

AsmEntry AsmCommands::asmBranchNode() const {
    return emitNodeEntry(NodeType::Branch);
}

AsmEntry AsmCommands::asmLoopNode() const {
    return emitNodeEntry(NodeType::Loop);
}

AsmEntry AsmCommands::asmRate(int rate) const {
    AsmEntry result = emitNodeEntry(NodeType::Rate);
    result.node->rate = rate;
    return result;
}

AsmEntry AsmCommands::asmSetPrecompFlags(unsigned int flags) const {
    AsmEntry result = emitNodeEntry(NodeType::PrecompFlags);
    result.node->precompFlags = flags;
    return result;
}

// =========================================================================
// Placeholder / waveform management
// =========================================================================

AsmEntry AsmCommands::asmSetVarPlaceholder(AsmRegister reg) {
    AsmEntry result = emitNodeEntry(NodeType::SetVarPlaceholder);
    result.node->reg = reg;
    return result;
}

AsmEntry AsmCommands::asmLockPlaceholder(std::shared_ptr<WaveformFront> wvf,
                                          int index) {
    AsmEntry result = emitNodeEntry(NodeType::LockPlaceholder);
    // Copy waveform name into node->strings[index]
    // TODO: wvf name access
    result.node->waveformIndex = index;
    return result;
}

AsmEntry AsmCommands::asmUnlockPlaceholder(std::shared_ptr<WaveformFront> wvf,
                                            int index) {
    AsmEntry result = emitNodeEntry(NodeType::UnlockPlaceholder);
    // Copy waveform name into node->strings[index]
    result.node->waveformIndex = index;
    return result;
}

AsmEntry AsmCommands::asmLoadPlaceholder() {
    return emitNodeEntry(NodeType::Load);
}

AsmEntry AsmCommands::asmPrefetch(std::shared_ptr<WaveformFront> wvf,
                                   int nameIndex, int regVal, int extraVal) {
    AsmEntry result = emitNodeEntry(NodeType::Prefetch);
    result.node->reg = static_cast<AsmRegister>(regVal);
    result.node->regVal = extraVal;

    if (wvf) {
        wvf->used = true;
        // Copy waveform name into node->strings[nameIndex]
    }

    result.node->waveformIndex = nameIndex;
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
    PlayConfig config;
    config.holdCount = holdCount;
    config.suppress = suppress;
    config.fourChannel = isFourChannelBool;
    config.trigger = trigger;
    config.precompFlag = 0;
    config.isBool = isBool;
    config.isHold = isHoldMode;

    WaveformFront* wf = wvf.get();
    if (!wf) {
        config.channelMask = 0;
        config.markerBits = 0;
        config.isDummy = true;
        return config;
    }

    config.isDummy = false;
    uint16_t sampleWidth = wf->sampleWidth;
    uint32_t fullMask = (1u << sampleWidth) - 1;
    uint32_t channelMask = (sampleWidth == 1) ? 2u : fullMask;
    if (!isHold) channelMask = fullMask;
    config.channelMask = channelMask;

    // Marker processing: iterate marker data pairwise from end
    // Computing (byte | (byte >> 1)) & 0x3 for each sample
    uint8_t* data = wf->markerData;
    size_t len = wf->markerDataEnd - data;
    uint16_t count = static_cast<uint16_t>(len);

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

AsmEntry AsmCommands::asmPlay(
    std::vector<std::shared_ptr<WaveformFront>> waveforms,
    int nameIndex, bool isHold, bool fourChannel, bool isBool,
    int holdCount, unsigned int suppress, bool isHoldMode,
    AsmRegister reg, int regVal, AsmRegister reg2,
    unsigned int trigger)
{
    AsmEntry result = emitNodeEntry(NodeType::Play);
    Node* node = result.node.get();

    node->waveformIndex = nameIndex;

    // Build waveform name vector
    for (auto& wvf : waveforms) {
        WaveformFront* wf = wvf.get();
        if (wf) {
            // node->strings.push_back(wf->getName());  // TODO: name access
            node->strings.push_back(std::nullopt);  // placeholder
        } else {
            node->strings.push_back(std::nullopt);
        }
    }

    // Compute play config
    std::shared_ptr<WaveformFront> currentWvf;
    if (nameIndex >= 0 && nameIndex < static_cast<int>(waveforms.size())) {
        currentWvf = waveforms[nameIndex];
    }

    node->playConfig = genPlayConfig(currentWvf, isHold, fourChannel,
                                      fourChannel, isBool, holdCount,
                                      suppress, isHoldMode, trigger);

    // Set register info
    node->reg = reg;
    node->reg2 = reg2;
    node->regVal = regVal;

    // Mark waveform as used and compute packed play word if needed
    if (currentWvf) {
        currentWvf->used = true;
        if (currentWvf->playIndex < 0) {
            currentWvf->playWord = node->playConfig.pack();
        }
    }

    return result;
}

AsmEntry AsmCommands::asmTable(int tableIndex, std::shared_ptr<WaveformFront> wvf,
                                int nameIndex, bool isHold, bool fourChannel,
                                int holdCount, unsigned int suppress,
                                bool isHoldMode, AsmRegister reg, int regVal) {
    AsmEntry result = emitNodeEntry(NodeType::Table);
    Node* node = result.node.get();

    node->playConfig = genPlayConfig(wvf, isHold, fourChannel, fourChannel,
                                      isHoldMode, holdCount, suppress,
                                      false, 0);

    node->reg = reg;
    node->regVal = regVal;

    if (wvf) {
        wvf->used = true;
        // Copy waveform name into node->strings[nameIndex]
    }

    node->waveformIndex = nameIndex;
    node->tableIndex = tableIndex;

    return result;
}

// =========================================================================
// Misc
// =========================================================================

AsmEntry AsmCommands::asmWtrigLSPlaceholder(int value) {
    AssemblerInstr instr;
    instr.cmd = Assembler::ST;
    instr.reg0 = AsmRegister::Reg(0);
    instr.immediates.emplace_back(value + 0x40);
    return emitEntry(instr);
}

AsmEntry AsmCommands::fb(int value) const {
    AssemblerInstr instr;
    instr.cmd = Assembler::FB;
    instr.immediates.emplace_back(value);
    return emitEntry(instr);
}

} // namespace zhinst
