// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Assembler — single instruction representation (~0x80 bytes)
//
// Binary nm confirms zhinst::Assembler is a single class with nested
// Command enum, static methods, and instance data/methods.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "zhinst/asm/asm_register.hpp"
#include "zhinst/ast/value.hpp"

namespace zhinst {

// The instruction representation constructed by AsmCommands methods.
// Size: 0x80 bytes (128 bytes).
//
//
//   +0x00  Command cmd           — opcode (Assembler::Command)
//   +0x04  (4 bytes padding)
//   +0x08  vector<Immediate>     — input operands (immediates)
//   +0x20  AsmRegister reg2      — READ source register (8 bytes: int + bool + pad)
//   +0x28  AsmRegister reg0      — WRITE destination register
//   +0x30  AsmRegister reg1      — dual-role register (read if cmdType∈{1,7}, written if cmdType==7)
//   +0x38  vector<Immediate>     — OUTPUT operands (also used for ADDI zero-check by simplifyAssign)
//   +0x50  std::string label     — branch target label (SSO: 32 bytes)
//   +0x68  std::string comment   — annotation / disassembly comment (SSO: 32 bytes)
//   +0x80  END
//
// Register semantic key (from getCmdType bitmask, ; renamed ):
//   regSrc (+0x20) is READ  when cmdType & 1
//   regDst (+0x28) is WRITTEN when cmdType & 2
//   regAux (+0x30) is READ when cmdType ∈ {1, 7}; WRITTEN when cmdType == 7
// Field rename completed (was reg2/reg0/reg1 → regSrc/regDst/regAux).
//! \brief Single AWG processor instruction record.
//!
//! `Assembler` is the in-memory representation of one assembled
//! instruction: an opcode (`cmd`) plus its register operands (a source,
//! a destination, and a dual-role auxiliary register), input and output
//! immediate-operand lists, an optional branch-target label, and an
//! annotation comment.  Per-opcode register semantics — which slots are
//! read, which are written — are derived by the static `getCmdType()`
//! helper from the `Command` enum value.
//!
//! Instances are produced by the per-method factories in `AsmCommands`
//! (with device-specific encoding choices delegated to
//! `AsmCommandsImpl`), collected into an `AsmList`, optimised by
//! `AsmOptimize`, and finally emitted as the on-wire opcode stream
//! consumed by `ElfWriter::addCode`.  `str(verbose)` produces the
//! human-readable disassembly that appears in the ELF `.asm` section.
class Assembler {
public:
    // Instruction opcodes. Values determined from disassembly of AsmCommands
    // methods and AsmCommandsImpl virtual dispatch.
    //! \brief AWG processor opcode set, packed into a 32-bit
    //! instruction-word prefix.
    //! \details The high nibble selects the opcode family (control,
    //! ALU register/immediate, load/store, branch, waveform, etc.);
    //! the low bits within each family disambiguate the specific
    //! mnemonic.  `INVALID` is the sentinel returned by
    //! `commandFromString()` when the lookup fails and is also the
    //! initial value of an unfilled `cmd` field.
    enum Command : uint32_t {
        // Control / pseudo
        END        = 0x00000000,  //!< End-of-program marker.
        NOP        = 0x00000001,  //!< No-op.
        LABEL      = 0x00000002,  //!< Pseudo-instruction emitted by the parser for label-only lines.
        MESSAGE    = 0x00000003,  //!< Inline-asm `msg` — emits a warning-level diagnostic at runtime.
        // 0x00000004 unused?
        ERROR_MSG  = 0x00000005,  //!< Inline-asm `rer` — emits an error-level diagnostic at runtime.

        // Waveform playback (short-opcode block)
        PRF        = 0x10000000,  //!< Prefetch waveform into cache.
        WVF        = 0x20000000,  //!< Play waveform (3-register form).
        WVFI       = 0x30000000,  //!< Play waveform, indexed (Cervino).
        WVFS_H     = 0x30000001,  //!< `wvfs` — set up waveform-fetch descriptor (Hirzel).

        // ALU — register + immediate (low 12 bits)
        ADDI       = 0x40000000,  //!< Add immediate to register.
        ADDIU      = 0x50000000,  //!< Add immediate (upper word, `imm << 12`) to register.

        // ALU — register-register
        ADDR       = 0x60000000,  //!< Register + register → destination register.
        SUBR       = 0x60000001,  //!< Register - register → destination register.
        ANDR       = 0x60000002,  //!< Register AND register → destination register.
        ORR        = 0x60000003,  //!< Register OR register → destination register.
        XNORR      = 0x60000004,  //!< Register XNOR register → destination register.
        SSL        = 0x60000005,  //!< Single-bit shift left.
        SSR        = 0x60000006,  //!< Single-bit shift right.
        XORR       = 0x60000007,  //!< Register XOR register → destination register.
                                  // Mnemonic confirmed via cmdMap entry "xorr".

        // ALU — register + immediate (low / upper)
        ANDI       = 0x70000000,  //!< AND register with immediate.
        ANDIU      = 0x80000000,  //!< AND register with immediate (upper word).
        ORI        = 0x90000000,  //!< OR register with immediate.
        ORIU       = 0xA0000000,  //!< OR register with immediate (upper word).
        XNORI      = 0xB0000000,  //!< XNOR register with immediate.
        XNORIU     = 0xC0000000,  //!< XNOR register with immediate (upper word).

        // Load / store / IO / sync barriers / extended-opcode block
        LD         = 0xD0000000,  //!< Load register from memory.
        WTRIG      = 0xE0000000,  //!< Wait for trigger event.
        WPRF       = 0xF0000000,  //!< Wait for prefetch (alias `wwvfq` in the cmd map).
        WWVF       = 0xF1000000,  //!< Wait for waveform completion.
                                  // Mnemonic confirmed via cmdMap entry "wwvf".
        CWVF       = 0xF2000000,  //!< Configure waveform — writes the PlayConfig register.
        BRZ        = 0xF3000000,  //!< Branch if register is zero.
        BRNZ       = 0xF4000000,  //!< Branch if register is non-zero.
        BRGZ       = 0xF5000000,  //!< Branch if register is greater than zero.
        ST         = 0xF6000000,  //!< Store register to memory.
        TRAP       = 0xF7000000,  //!< Trap / fault.
        IRPT       = 0xF8000000,  //!< Raise interrupt.
        CWVFR      = 0xF9000000,  //!< Configure waveform with register operand.
        WVFE       = 0xFA000000,  //!< Waveform end (1-register, extended-opcode form).
        WVFEI      = 0xFB000000,  //!< Waveform end, interleaved.
                                  // Was: UNK_FB (placeholder name in earlier passes).
        WVFET      = 0xFC000000,  //!< Waveform end, triggered.
                                  // Was: WVFT_H (placeholder name in earlier passes).
        WTRIGI     = 0xFD000000,  //!< Wait for trigger, indexed.
        JMP        = 0xFE000000,  //!< Unconditional jump.
                                  // Was: BRZ_H (placeholder name in earlier passes; see notes/opcode_map.md).
        FB         = 0xFF000000,  //!< Feedback / status-register read.

        // Sentinel
        INVALID    = 0xFFFFFFFF,  //!< Sentinel for unrecognised mnemonics and unfilled command slots.
    };

    //! \brief Discriminator for placeholder play-zero forms emitted
    //! during the dummy-waveform pre-pass.
    enum class PlayDummyType : int {
        Type0 = 0,  //!< Dummy form 0.
        Type1 = 1,  //!< Dummy form 1.
    };

    // CmdType — command type category from getCmdType() (A10)
    // Bits: 0=reads registers, 1=writes dest register
    //! \brief Bitmask returned by `getCmdType()` describing how each
    //! opcode uses its register operand slots.
    //! \details Bit 0 set → the instruction reads its source
    //! register slot(s); bit 1 set → the instruction writes the
    //! destination register slot.  The combined value `7`
    //! (`CmdType_RegReg`) flags reg-reg ALU ops where the auxiliary
    //! register slot is both read and written.
    enum CmdType : int {
        CmdType_None      = 0,  //!< No register access (NOP, JMP, control-flow).
        CmdType_Read      = 1,  //!< Reads registers (PRF, WVF, BRZ, ST, etc.).
        CmdType_Write     = 2,  //!< Writes destination register (LD).
        CmdType_ReadWrite = 3,  //!< Reads source and writes destination (ALU-immediate).
        CmdType_RegReg    = 7,  //!< Reg-reg form: `regAux` is both source and destination (ADDR..XORR).
    };

    // RegOrder — register ordering from getRegisterOrder() (A11)
    //! \brief Operand layout returned by `getRegisterOrder()`,
    //! describing which register slots the encoder must populate.
    enum class RegOrder : int {
        None       = 0,  //!< No register operands.
        SourceOnly = 1,  //!< Single register goes into `regSrc`.
        DestOnly   = 2,  //!< Single register goes into `regDst`.
        ThreeReg   = 3,  //!< Two registers go into `regAux` and `regSrc`.
        DestImmSrc = 4,  //!< Two registers go into `regDst` and `regSrc`.
    };

    // OpcodeFormat — encoding format categories for opcode dispatch (A8)
    //! \brief Bit-field encoding category dispatched per opcode by
    //! the assembler back-end.
    enum class OpcodeFormat : int {
        NoArg        = 0,  //!< Base opcode unchanged (END, NOP).
        RegImm20     = 1,  //!< `reg << 24 | imm20` (ADDI, ADDIU, ANDI, ...).
        RegTripleImm8= 2,  //!< `reg << 24 | 3 × imm8` (PRF, WVF, WVFI, WTRIG).
        DualRegImm20 = 3,  //!< `reg1 << 24 | reg2 << 20 | imm20` (ADDR..XORR, WVFS_H).
        Complex      = 4,  //!< Variable child layout (branches, load/store, control).
        DualImm14    = 5,  //!< `base | imm14 << 14 | imm14` (waveform table addressing).
    };

    // Static methods
    //! \brief Returns the canonical mnemonic string for a command.
    //! \details Performs a linear reverse scan over the global
    //! command map, so the cost is `O(n)` in the map size.
    //! \param cmd Opcode to look up.
    //! \return Lowercase mnemonic string, or the empty string if
    //! no entry matches `cmd`.
    static std::string commandToString(Command cmd);    // 0x28f7f0
    //! \brief Resolves a mnemonic string to its `Command` opcode.
    //! \details The lookup is case-insensitive (`name` is
    //! lowercased before the map probe).
    //! \param name Mnemonic string (any case) to resolve.
    //! \return The matching opcode, or `INVALID` if `name` is not
    //! a recognised mnemonic.
    static Command commandFromString(const std::string& name);  // 0x2902f0
    //! \brief Classifies an opcode into a scheduling category used
    //! by the optimiser and pipeline tracker.
    //! \param cmd Opcode to classify.
    //! \return Numeric category in `{0, 1, 3, 4}`; `0` for
    //! unrecognised opcodes.
    static int getOpcodeType(Command cmd);   // 0x28f8a0
    //! \brief Returns the cycle count consumed by an opcode.
    //! \param cmd Opcode to measure.
    //! \return Cycle count: `kCycle_Single` for most opcodes,
    //! `kCycle_BranchPenalty` for taken-branch variants, or
    //! `kCycle_Unknown` for unmodelled opcodes.
    static int getCycles(Command cmd);       // 0x28fac0
    //! \brief Returns the register-usage bitmask for an opcode.
    //! \param cmd Opcode to classify.
    //! \return One of the `CmdType` values describing which
    //! register slots are read or written.
    static int getCmdType(Command cmd);      // 0x28fce0
    //! \brief Returns the register-slot ordering for an opcode.
    //! \param cmd Opcode to classify.
    //! \return One of the `RegOrder` values describing how the
    //! parsed register operands map onto the `regSrc` / `regDst` /
    //! `regAux` slots.
    static int getRegisterOrder(Command cmd); // 0x28fe70

    // Constants
    static constexpr int kCycle_Unknown       = 0;  //!< Cycle count returned for unmodelled opcodes.
    static constexpr int kCycle_Single        = 1;  //!< Cycle count for ordinary single-cycle opcodes.
    static constexpr int kCycle_BranchPenalty = 3;  //!< Cycle count for taken-branch instructions (BRZ, BRNZ, BRGZ).

    // Instance data (same layout as original struct)
    Command cmd = INVALID;                    //!< Opcode for this instruction; `INVALID` until set by a factory.
    // 4 bytes padding
    std::vector<Immediate> immediates;        //!< Input operand list (immediates).
    AsmRegister regSrc;                       //!< Source register slot — read when `getCmdType() & 1`.
    AsmRegister regDst;                       //!< Destination register slot — written when `getCmdType() & 2`.
    AsmRegister regAux;                       //!< Auxiliary register slot — read for `CmdType ∈ {1,7}`, also written for `CmdType == 7`.
    std::vector<Immediate> outputs;           //!< Output operand list; also used by `simplifyAssign` for ADDI-zero checks.
    std::string label;                        //!< Branch target label (empty for non-branch instructions).
    std::string comment;                      //!< Annotation appended to the disassembly when `str(verbose=true)` runs.

    // Compiler-generated special members match binary layout.
    // Binary addresses: dtor @0x103980, copy-assign @0x125e80,
    //                   move-assign @0x125ab0.
    //! \brief Constructs an empty instruction with `cmd = INVALID`
    //! and all operand slots default-initialised.
    Assembler() = default;
    //! \brief Releases the owned operand vectors and strings.
    ~Assembler();
    //! \brief Copy-constructs an instruction by duplicating every
    //! operand slot.
    Assembler(const Assembler&) = default;
    //! \brief Copy-assigns from another instruction.
    //! \param other Source instruction to copy from.
    //! \return Reference to `*this`.
    Assembler& operator=(const Assembler& other);
    //! \brief Move-assigns from another instruction.
    //! \param other Source instruction; left in a valid but
    //! unspecified state.
    //! \return Reference to `*this`.
    Assembler& operator=(Assembler&& other) noexcept;

    // 0x28ffe0 — returns packed int64_t: (1<<32)|regIndex if valid regs, else 0
    //! \brief Returns the largest register index referenced by any
    //! of this instruction's register slots.
    //! \details Examines `regSrc`, `regDst`, and `regAux`, ignoring
    //! slots whose `valid` flag is clear.  The high 32 bits encode
    //! a "found a register" flag so callers can distinguish "no
    //! registers" from "register R0".
    //! \return `0` when no slot is valid; otherwise
    //! `(1LL << 32) | maxIndex` where `maxIndex` is the highest
    //! valid register number.
    //! \binarynote The full int value of the maximum register
    //! number is preserved in the low 32 bits; an earlier
    //! reconstruction truncated to `uint8_t`, which silently
    //! aliased registers ≥ 256 (see IF-191).
    int64_t highestRegisterNumber() const;

    // 0x28ebd0 — produces disassembly string.
    //! \brief Renders this instruction as one line of textual
    //! disassembly suitable for the ELF `.asm` section.
    //! \details Pure-label entries (`cmd == LABEL`) format as
    //! `"label:"`.  Everything else emits the mnemonic followed
    //! by input immediates, register slots in `regDst`, `regAux`,
    //! `regSrc` order, output immediates, and the branch label,
    //! with the trailing comma stripped.  In `verbose` mode the
    //! `comment` field is right-padded onto the same line.
    //! \param verbose When true, appends `// comment` after the
    //! operand list (padded to column 25).
    //! \return Single-line disassembly string.
    std::string str(bool verbose) const;
};

// noOpt: (cmd - 3) < 3u, i.e. cmd in {MESSAGE=3, 4, ERROR_MSG=5}
//! \brief Returns true for diagnostic-emitting pseudo-instructions
//! that the optimiser must leave untouched.
//! \details Matches `MESSAGE` (3), the unused opcode 4, and
//! `ERROR_MSG` (5) — the diagnostic pseudo-ops whose ordering
//! relative to surrounding instructions must be preserved.
//! \param instr Instruction to test.
//! \return `true` when the instruction is a diagnostic pseudo-op.
inline bool noOpt(const Assembler& instr) {
    return (static_cast<uint32_t>(instr.cmd) - 3u) < 3u;
}

// AsmOperationType — classifies operand slots in instruction output.
// str() @0x28d280 maps: 0→"cmd", 1→"name", 2→"value", 3→"reg", else→"?".
//! \brief Operand-slot classification used when serialising an
//! `AsmList` to text or JSON.
enum class AsmOperationType : int {
    Cmd   = 0,  //!< Command/opcode slot.
    Name  = 1,  //!< Identifier slot (label or symbol).
    Value = 2,  //!< Numeric immediate slot.
    Reg   = 3,  //!< Register slot.
};
//! \brief Returns the canonical string for an `AsmOperationType`.
//! \param t Slot classification to render.
//! \return One of `"cmd"`, `"name"`, `"value"`, `"reg"`, or `"?"`
//! for unrecognised values.
std::string str(AsmOperationType t);  // @0x28d280

} // namespace zhinst
