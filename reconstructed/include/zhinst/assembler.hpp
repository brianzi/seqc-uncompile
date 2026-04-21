// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AssemblerInstr — single instruction representation (~0x80 bytes)
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "asm_register.hpp"
#include "value.hpp"

namespace zhinst {

namespace Assembler {

// Instruction opcodes. Values determined from disassembly of AsmCommands
// methods and AsmCommandsImpl virtual dispatch.
enum Command : uint32_t {
    // Control / pseudo
    END        = 0x00000000,
    NOP        = 0x00000001,
    LABEL      = 0x00000002,
    MESSAGE    = 0x00000003,
    // 0x00000004 unused?
    ERROR_MSG  = 0x00000005,

    // Waveform playback
    PRF        = 0x10000000,
    WVF        = 0x20000000,  // Cervino, or Hirzel with marker reg
    WVFI       = 0x30000000,  // Cervino interleaved
    WVFS_H     = 0x30000001,  // Hirzel wvfs (special/dummy)

    // ALU — signed immediate
    ADDI       = 0x40000000,
    ADDIU      = 0x50000000,  // unsigned immediate

    // ALU — register-register
    ADDR       = 0x60000000,
    SUBR       = 0x60000001,
    ANDR       = 0x60000002,
    ORR        = 0x60000003,
    XNORR      = 0x60000004,
    SSL        = 0x60000005,  // shift left
    SSR        = 0x60000006,  // shift right
    XORR       = 0x60000007,  // xor register-register (confirmed: cmdMap "xorr")

    // ALU — signed immediate (other ops)
    ANDI       = 0x70000000,
    ANDIU      = 0x80000000,
    ORI        = 0x90000000,
    ORIU       = 0xA0000000,
    XNORI      = 0xB0000000,
    XNORIU     = 0xC0000000,

    // Load / store / IO
    LD         = 0xD0000000,
    WTRIG      = 0xE0000000,
    WPRF       = 0xF0000000,  // also "wwvfq" alias in cmdMap
    WWVF       = 0xF1000000,  // write waveform (confirmed: cmdMap "wwvf")
    CWVF       = 0xF2000000,
    BRZ        = 0xF3000000,  // branch if zero
    BRNZ       = 0xF4000000,  // branch if not zero
    BRGZ       = 0xF5000000,  // branch if greater than zero (NEW — Phase 2a)
    ST         = 0xF6000000,
    TRAP       = 0xF7000000,
    IRPT       = 0xF8000000,
    CWVFR      = 0xF9000000,  // cancel waveform (register)
    WVFE       = 0xFA000000,  // waveform end (was: WVF_H)
    WVFEI      = 0xFB000000,  // waveform end interleaved (was: UNK_FB)
    WVFET      = 0xFC000000,  // waveform end triggered (was: WVFT_H)
    WTRIGI     = 0xFD000000,
    JMP        = 0xFE000000,  // unconditional jump (was: BRZ_H)
    FB         = 0xFF000000,

    // Sentinel
    INVALID    = 0xFFFFFFFF,
};

enum class PlayDummyType : int {
    Type0 = 0,
    Type1 = 1,
};

// Convert opcode to string name (for error messages / disassembly).
// Reconstructed from commandToString() — uses global cmdMap at 0xb84c20.
std::string commandToString(Command cmd);

// Reverse lookup: string to Command. Returns INVALID if not found.
// Input is lowercased before lookup.
// 0x2902f0
Command commandFromString(const std::string& name);

// getOpcodeType() — classify opcode for scheduling/analysis
int getOpcodeType(Command cmd);

// getCycles() — return cycle count for this instruction
int getCycles(Command cmd);

// getCmdType() — return command type category
int getCmdType(Command cmd);

// getRegisterOrder() — return register read/write ordering info
int getRegisterOrder(Command cmd);

} // namespace Assembler

// The instruction representation constructed by AsmCommands methods.
// Size: 0x80 bytes (128 bytes).
//
// CORRECTED layout (Phase 2 — confirmed from copy ctor, move assignment,
// destructor, str(), and multiple AsmCommands method sites):
//
//   +0x00  Command cmd           — opcode (Assembler::Command)
//   +0x04  (4 bytes padding)
//   +0x08  vector<Immediate>     — input operands (immediates)
//   +0x20  AsmRegister reg2      — DESTINATION register (8 bytes: int + bool + pad)
//   +0x28  AsmRegister reg0      — source register 1
//   +0x30  AsmRegister reg1      — source register 2
//   +0x38  vector<Immediate>     — OUTPUT operands (was previously unknown +0x38..+0x4F!)
//   +0x50  std::string label     — branch target label (SSO: 32 bytes)
//   +0x68  std::string comment   — annotation / disassembly comment (SSO: 32 bytes)
//   +0x80  END
//
// Key correction from Phase 1: registers are reg2(dest)/reg0(src1)/reg1(src2),
// NOT reg0/reg1/reg2. The field at +0x38 is a second vector<Immediate> for
// output operands, NOT part of the label string.
struct AssemblerInstr {
    Assembler::Command cmd = Assembler::INVALID;  // +0x00
    // 4 bytes padding
    std::vector<Immediate> immediates;    // +0x08 — input operands
    AsmRegister reg2;                     // +0x20 — destination register (aka "dest")
    AsmRegister reg0;                     // +0x28 — source register 1
    AsmRegister reg1;                     // +0x30 — source register 2
    std::vector<Immediate> outputs;       // +0x38 — output operands
    std::string label;                    // +0x50 — branch target label
    std::string comment;                  // +0x68 — annotation string

    // 0x28ffe0 — returns packed int64_t: (1<<32)|regIndex if valid regs, else 0
    int64_t highestRegisterNumber() const;

    // 0x28ebd0 — produces disassembly string.
    // If cmd == LABEL, returns "label:" directly.
    // Otherwise: "\t" + cmdName + inputs + reg0 + reg1 + reg2 + outputs + label
    // If verbose && comment non-empty, appends "// comment"
    std::string str(bool verbose) const;
};

// isWaveformCmd: (cmd - 3) < 3u, i.e. cmd in {MESSAGE=3, 4, ERROR_MSG=5}
inline bool isWaveformCmd(const AssemblerInstr& instr) {
    return (static_cast<uint32_t>(instr.cmd) - 3u) < 3u;
}

} // namespace zhinst
