// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommands — main assembler code generator class
// ============================================================================
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "asm_commands_impl.hpp"
#include "asm_list.hpp"
#include "asm_register.hpp"
#include "assembler.hpp"
#include "play_config.hpp"
#include "value.hpp"
#include "waveform_front.hpp"
#include "wavetable_front.hpp"

namespace zhinst {

// AsmCommands — generates assembler instruction records for the AWG processor.
//
// Member layout (approximate, from disassembly offset analysis):
//   +0x00  (vtable ptr or base class data)
//   +0x10  unique_ptr<AsmCommandsImpl> impl_
//   +0x20  shared_ptr<WavetableFront> wavetable_  (approx)
//   +0x40  function<void(const string&)> errorHandler_
//   +0x50  int wavetableFrontIndex_  (line number / context index)
//   +0x54  int nodeExtraRef_         (passed to Node constructors)
class AsmCommands {
public:
    // Constructor (from construct_at signature in binary):
    // AsmCommands(const AWGCompilerConfig& config,
    //             shared_ptr<WavetableFront> wavetable,
    //             function<void(const string&)> errorHandler);

    // =====================================================================
    // Waveform playback
    // =====================================================================

    AsmEntry prf(AsmRegister reg1, AsmRegister reg2, int intArg) const;
    AsmEntry wprf() const;
    AsmEntry wwvfq() const;
    AsmEntry wwvf() const;
    AsmEntry wvf(AsmRegister reg, AsmRegister dstReg, int length) const;
    AsmEntry wvfi(AsmRegister reg, AsmRegister dstReg, int length) const;
    AsmEntry wvfs(Assembler::PlayDummyType type, AsmRegister reg, int length) const;
    AsmEntry wvft(AsmRegister reg, int length) const;
    AsmEntry cwvf(int value) const;
    AsmEntry cwvfr(AsmRegister reg) const;

    // =====================================================================
    // Branch
    // =====================================================================

    AsmEntry br(const std::string& label, bool flag) const;
    AsmEntry brz(AsmRegister reg, const std::string& label, bool flag) const;
    AsmEntry brnz(AsmRegister reg, const std::string& label, bool flag) const;
    AsmEntry brgz(AsmRegister reg, const std::string& label, bool flag) const;

    // =====================================================================
    // ALU register-register
    // =====================================================================

    AsmEntry alur(Assembler::Command cmd, AsmRegister dst, AsmRegister src) const;
    AsmEntry addr(AsmRegister dst, AsmRegister src) const;
    AsmEntry subr(AsmRegister dst, AsmRegister src) const;
    AsmEntry andr(AsmRegister dst, AsmRegister src) const;
    AsmEntry orr(AsmRegister dst, AsmRegister src) const;
    AsmEntry xnorr(AsmRegister dst, AsmRegister src) const;

    // =====================================================================
    // Shift (delegates to impl_)
    // =====================================================================

    AsmEntry ssl(AsmRegister reg) const;
    AsmEntry ssr(AsmRegister reg) const;

    // =====================================================================
    // ALU immediate-unsigned (single instruction)
    // =====================================================================

    AsmEntry aluiu(Assembler::Command cmd, AsmRegister dst,
                   AsmRegister src, Immediate imm) const;
    AsmEntry addiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    AsmEntry andiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    AsmEntry oriu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    AsmEntry xnoriu(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU immediate-signed (may emit multiple instructions)
    // =====================================================================

    std::vector<AsmEntry> alui(Assembler::Command cmd, AsmRegister dst,
                               AsmRegister src, Immediate imm) const;
    std::vector<AsmEntry> addi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmEntry> addi32(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmEntry> andi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmEntry> ori(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmEntry> xnori(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU with Value (runtime variant) — converts to int, then delegates
    // =====================================================================

    int toInt32(Value val) const;

    std::vector<AsmEntry> addi(AsmRegister dst, AsmRegister src, Value val) const;
    AsmEntry addiu(AsmRegister dst, AsmRegister src, Value val) const;
    std::vector<AsmEntry> andi(AsmRegister dst, AsmRegister src, Value val) const;
    AsmEntry andiu(AsmRegister dst, AsmRegister src, Value val) const;
    std::vector<AsmEntry> ori(AsmRegister dst, AsmRegister src, Value val) const;
    AsmEntry oriu(AsmRegister dst, AsmRegister src, Value val) const;
    std::vector<AsmEntry> xnori(AsmRegister dst, AsmRegister src, Value val) const;
    AsmEntry xnoriu(AsmRegister dst, AsmRegister src, Value val) const;

    // =====================================================================
    // Load / Store
    // =====================================================================

    AsmEntry ld(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    AsmEntry st(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    AsmEntry ldio(AsmRegister reg, bool highBank) const;
    AsmEntry sdio(AsmRegister reg, bool highBank) const;
    AsmEntry luser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    AsmEntry suser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Trigger
    // =====================================================================

    AsmEntry ltrig(AsmRegister reg) const;
    AsmEntry strig(AsmRegister reg) const;
    AsmEntry sinttrig(AsmRegister reg) const;
    AsmEntry wtrig(AsmRegister r1, AsmRegister r2) const;
    AsmEntry wtrigi(int value) const;

    // =====================================================================
    // Misc I/O
    // =====================================================================

    AsmEntry sid(AsmRegister reg, bool highBank) const;
    AsmEntry smap(AsmRegister r1, AsmRegister r2, int arg) const;
    AsmEntry ldiotrig(AsmRegister reg) const;
    AsmEntry lcnt(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Control flow / special
    // =====================================================================

    AsmEntry trap() const;
    AsmEntry irpt() const;
    AsmEntry end() const;
    AsmEntry nop() const;

    // =====================================================================
    // Sync
    // =====================================================================

    AsmEntry syncCervino(AsmRegister reg1, AsmRegister reg2, bool flag) const;
    AsmEntry unsyncCervino() const;
    AsmEntry asmSyncPlaceholderCervino() const;
    AsmEntry asmSyncHirzel() const;

    // =====================================================================
    // Pseudo-instructions / directives
    // =====================================================================

    AsmEntry asmZero(AsmRegister reg) const;
    AsmEntry asmOne(AsmRegister reg) const;
    AsmEntry asmLabel(const std::string& label) const;
    AsmEntry asmMessage(const std::string& msg, bool isError) const;

    // =====================================================================
    // Node-creating commands
    // =====================================================================

    AsmEntry asmBranchNode() const;
    AsmEntry asmLoopNode() const;
    AsmEntry asmRate(int rate) const;
    AsmEntry asmSetPrecompFlags(unsigned int flags) const;

    // =====================================================================
    // Placeholder / waveform management (non-const)
    // =====================================================================

    AsmEntry asmSetVarPlaceholder(AsmRegister reg);
    AsmEntry asmLockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    AsmEntry asmUnlockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    AsmEntry asmLoadPlaceholder();
    AsmEntry asmPrefetch(std::shared_ptr<WaveformFront> wvf,
                         int nameIndex, int regVal, int extraVal);

    // =====================================================================
    // Play / Table
    // =====================================================================

    PlayConfig genPlayConfig(const std::shared_ptr<WaveformFront>& wvf,
                             bool isHold, bool fourChannel, bool isFourChannelBool,
                             bool isBool, int holdCount, unsigned int suppress,
                             bool isHoldMode, unsigned int trigger) const;

    AsmEntry asmPlay(std::vector<std::shared_ptr<WaveformFront>> waveforms,
                     int nameIndex, bool isHold, bool fourChannel, bool isBool,
                     int holdCount, unsigned int suppress, bool isHoldMode,
                     AsmRegister reg, int regVal, AsmRegister reg2,
                     unsigned int trigger);

    AsmEntry asmTable(int tableIndex, std::shared_ptr<WaveformFront> wvf,
                      int nameIndex, bool isHold, bool fourChannel,
                      int holdCount, unsigned int suppress, bool isHoldMode,
                      AsmRegister reg, int regVal);

    // =====================================================================
    // Misc
    // =====================================================================

    AsmEntry asmWtrigLSPlaceholder(int value);
    AsmEntry fb(int value) const;

private:
    std::unique_ptr<AsmCommandsImpl> impl_;                      // +0x10
    std::shared_ptr<WavetableFront> wavetable_;                  // ~+0x20
    std::function<void(const std::string&)> errorHandler_;       // ~+0x40
    int wavetableFrontIndex_ = 0;                                // +0x50
    int nodeExtraRef_ = 0;                                       // +0x54

    // Helper: build an AsmEntry from a local AssemblerInstr
    AsmEntry emitEntry(const AssemblerInstr& instr) const;
    AsmEntry emitEntry(const AssemblerInstr& instr, int overrideWavetableFront) const;

    // Helper: build an AsmEntry with a Node
    AsmEntry emitNodeEntry(NodeType type) const;
};

} // namespace zhinst
