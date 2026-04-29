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
//   +0x50  int wavetableFrontIndex_  (line number / wavetable context; init=0)
//   +0x54  int numChannelGroups_     (from AWGCompilerConfig+0x1c; 1/2/4; pre-alloc size for Node vectors)
class AsmCommands {
public:
    // Constructor — creates impl via AsmCommandsImpl::getInstance()
    // Binary: 0x11d080 (inline in Compiler ctor)
    AsmCommands(const AWGCompilerConfig& config,
                std::shared_ptr<WavetableFront> wavetable,
                std::function<void(const std::string&)> errorHandler);

    // =====================================================================
    // Waveform playback
    // =====================================================================

    AsmList::Asm prf(AsmRegister reg1, AsmRegister reg2, int intArg) const;
    AsmList::Asm wprf() const;
    AsmList::Asm wwvfq() const;
    AsmList::Asm wwvf() const;
    AsmList::Asm wvf(AsmRegister reg, AsmRegister dstReg, int length) const;
    AsmList::Asm wvfi(AsmRegister reg, AsmRegister dstReg, int length) const;
    AsmList::Asm wvfs(Assembler::PlayDummyType type, AsmRegister reg, int length) const;
    AsmList::Asm wvft(AsmRegister reg, int length) const;
    AsmList::Asm cwvf(int value) const;
    AsmList::Asm cwvfr(AsmRegister reg) const;

    // =====================================================================
    // Branch
    // =====================================================================

    AsmList::Asm br(const std::string& label, bool noOpt) const;
    AsmList::Asm brz(AsmRegister reg, const std::string& label, bool noOpt) const;
    AsmList::Asm brnz(AsmRegister reg, const std::string& label, bool noOpt) const;
    AsmList::Asm brgz(AsmRegister reg, const std::string& label, bool noOpt) const;

    // =====================================================================
    // ALU register-register
    // =====================================================================

    AsmList::Asm alur(Assembler::Command cmd, AsmRegister dst, AsmRegister src) const;
    AsmList::Asm addr(AsmRegister dst, AsmRegister src) const;
    AsmList::Asm subr(AsmRegister dst, AsmRegister src) const;
    AsmList::Asm andr(AsmRegister dst, AsmRegister src) const;
    AsmList::Asm orr(AsmRegister dst, AsmRegister src) const;
    AsmList::Asm xnorr(AsmRegister dst, AsmRegister src) const;

    // =====================================================================
    // Shift (delegates to impl_)
    // =====================================================================

    AsmList::Asm ssl(AsmRegister reg) const;
    AsmList::Asm ssr(AsmRegister reg) const;

    // =====================================================================
    // ALU immediate-unsigned (single instruction)
    // =====================================================================

    AsmList::Asm aluiu(Assembler::Command cmd, AsmRegister dst,
                   AsmRegister src, Immediate imm) const;
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    AsmList::Asm xnoriu(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU immediate-signed (may emit multiple instructions)
    // =====================================================================

    std::vector<AsmList::Asm> alui(Assembler::Command cmd, AsmRegister dst,
                               AsmRegister src, Immediate imm) const;
    std::vector<AsmList::Asm> addi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmList::Asm> addi32(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmList::Asm> andi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmList::Asm> ori(AsmRegister dst, AsmRegister src, Immediate imm) const;
    std::vector<AsmList::Asm> xnori(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU with Value (runtime variant) — converts to int, then delegates
    // =====================================================================

    int toInt32(Value val) const;

    std::vector<AsmList::Asm> addi(AsmRegister dst, AsmRegister src, Value val) const;
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Value val) const;
    std::vector<AsmList::Asm> andi(AsmRegister dst, AsmRegister src, Value val) const;
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Value val) const;
    std::vector<AsmList::Asm> ori(AsmRegister dst, AsmRegister src, Value val) const;
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Value val) const;
    std::vector<AsmList::Asm> xnori(AsmRegister dst, AsmRegister src, Value val) const;
    AsmList::Asm xnoriu(AsmRegister dst, AsmRegister src, Value val) const;

    // =====================================================================
    // Load / Store
    // =====================================================================

    AsmList::Asm ld(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    AsmList::Asm st(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    AsmList::Asm ldio(AsmRegister reg, bool highBank) const;
    AsmList::Asm sdio(AsmRegister reg, bool highBank) const;
    AsmList::Asm luser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    AsmList::Asm suser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Trigger
    // =====================================================================

    AsmList::Asm ltrig(AsmRegister reg) const;
    AsmList::Asm strig(AsmRegister reg) const;
    AsmList::Asm sinttrig(AsmRegister reg) const;
    AsmList::Asm wtrig(AsmRegister r1, AsmRegister r2) const;
    AsmList::Asm wtrigi(int value) const;

    // =====================================================================
    // Misc I/O
    // =====================================================================

    AsmList::Asm sid(AsmRegister reg, bool highBank) const;
    std::vector<AsmList::Asm> smap(AsmRegister r1, AsmRegister r2, int arg) const;
    AsmList::Asm ldiotrig(AsmRegister reg) const;
    AsmList::Asm lcnt(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Control flow / special
    // =====================================================================

    AsmList::Asm trap() const;
    AsmList::Asm irpt() const;
    AsmList::Asm end() const;
    AsmList::Asm nop() const;

    // =====================================================================
    // Sync
    // =====================================================================

    AsmList syncCervino(AsmRegister reg1, AsmRegister reg2, bool flag) const;
    AsmList unsyncCervino() const;  // returns 2 ST entries (not single AsmList::Asm)
    AsmList::Asm asmSyncPlaceholderCervino() const;
    AsmList::Asm asmSyncHirzel() const;

    // =====================================================================
    // Pseudo-instructions / directives
    // =====================================================================

    AsmList::Asm asmZero(AsmRegister reg) const;
    AsmList::Asm asmOne(AsmRegister reg) const;
    AsmList::Asm asmLabel(const std::string& label) const;
    AsmList::Asm asmMessage(const std::string& msg, bool isError) const;

    // =====================================================================
    // Node-creating commands
    // =====================================================================

    AsmList::Asm asmBranchNode() const;
    AsmList::Asm asmLoopNode() const;
    AsmList::Asm asmRate(int rate) const;
    AsmList::Asm asmSetPrecompFlags(unsigned int flags) const;

    // =====================================================================
    // Placeholder / waveform management (non-const)
    // =====================================================================

    AsmList::Asm asmSetVarPlaceholder(AsmRegister reg);
    AsmList::Asm asmLockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    AsmList::Asm asmUnlockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    AsmList::Asm asmLoadPlaceholder();
    AsmList::Asm asmPrefetch(std::shared_ptr<WaveformFront> wvf,
                         int nameIndex, int regVal, int extraVal);

    // =====================================================================
    // Play / Table
    // =====================================================================

    PlayConfig genPlayConfig(const std::shared_ptr<WaveformFront>& wvf,
                             bool isHold, bool fourChannel, bool playNow,
                             bool hold, int rate, unsigned int suppress,
                             bool is4Channel, unsigned int trigger) const;

    AsmList::Asm asmPlay(std::vector<std::shared_ptr<WaveformFront>> waveforms,
                     int deviceIndex, bool isHold, bool fourChannel, bool hold,
                     int rate, unsigned int suppress, bool is4Channel,
                     AsmRegister lengthReg, int length, AsmRegister reg2,
                     unsigned int trigger);

    AsmList::Asm asmTable(int tableIndex, std::shared_ptr<WaveformFront> wvf,
                      int deviceIndex, bool isHold, bool fourChannel,
                      int rate, unsigned int suppress, bool is4Channel,
                      AsmRegister lengthReg, int length);

    // =====================================================================
    // Misc
    // =====================================================================

    AsmList::Asm asmWtrigLSPlaceholder(int value);
    AsmList::Asm fb(int value) const;

    // Setter for the dual-purpose wavetable-front / source-line index at
    // +0x50. Used by Prefetch::placeSingleCommand (0x1d79bf) to propagate
    // the placeholder's `wavetableFront` value into the active assembler
    // context before emitting commands.
    void setWavetableFrontIndex(int value) { wavetableFrontIndex_ = value; }
    int  wavetableFrontIndex() const { return wavetableFrontIndex_; }

private:
    std::unique_ptr<AsmCommandsImpl> impl_;                      // +0x10
    std::shared_ptr<WavetableFront> wavetable_;                  // ~+0x20
    std::function<void(const std::string&)> errorHandler_;       // ~+0x40
    int wavetableFrontIndex_ = 0;                                // +0x50
    int numChannelGroups_ = 0;                                   // +0x54

    // Helper: build an AsmList::Asm from a local AssemblerInstr
    AsmList::Asm emitEntry(const AssemblerInstr& instr) const;
    AsmList::Asm emitEntry(const AssemblerInstr& instr, int overrideWavetableFront) const;

    // Helper: build an AsmList::Asm with a Node
    AsmList::Asm emitNodeEntry(NodeType type) const;
};

} // namespace zhinst
