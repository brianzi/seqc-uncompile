// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImpl — virtual base + Cervino/Hirzel declarations
// ============================================================================
#pragma once

#include <memory>
#include <string>

#include "zhinst/asm/asm_list.hpp"
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/asm/assembler.hpp"
#include "zhinst/core/types.hpp"

namespace zhinst {

// Abstract base class for device-specific instruction encoding.
// Used as a pimpl inside AsmCommands.
//
// Vtable layout (13 slots):
//   [0]  ~AsmCommandsImpl() (deleting dtor)
//   [1]  ~AsmCommandsImpl() (complete dtor)
//   [2]  isCWVFRSupported() const -> bool
//   [3]  wwvfq(int) const
//   [4]  wprf(int) const
//   [5]  wvf(AsmRegister, AsmRegister, int, int) const
//   [6]  wvfi(AsmRegister, AsmRegister, int, int) const
//   [7]  wvfs(PlayDummyType, AsmRegister, int, int) const
//   [8]  wvft(AsmRegister, int, int) const
//   [9]  brz(AsmRegister, const string&, bool, int) const
//   [10] ssl(AsmRegister, int) const
//   [11] ssr(AsmRegister, int) const
//   [12] ldiotrig(AsmRegister, int) const
//! \brief Device-specific instruction-encoding back end for `AsmCommands`.
//!
//! `AsmCommandsImpl` is the polymorphic pimpl behind `AsmCommands`:
//! each AWG hardware family (Cervino, Hirzel) implements the abstract
//! virtuals here with the opcode bit-patterns and operand layouts its
//! processor expects.  The `getInstance(deviceType)` factory selects
//! the right concrete subclass based on `AwgDeviceType`.
//!
//! Methods that have no encoding on a given family (for example
//! Cervino lacks the wavetable-quad write) throw an exception when
//! called on that subclass.  `isCWVFRSupported()` advertises whether
//! the conditional-waveform-with-register-cancel form is available so
//! the front end can choose between equivalent emission strategies.
class AsmCommandsImpl {
public:
    virtual ~AsmCommandsImpl();

    virtual bool isCWVFRSupported() const = 0;

    virtual AsmList::Asm wwvfq(int lineNumber) const = 0;
    virtual AsmList::Asm wprf(int lineNumber) const = 0;

    virtual AsmList::Asm wvf(AsmRegister waveReg, AsmRegister dstReg,
                         int length, int lineNumber) const = 0;
    virtual AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister dstReg,
                          int length, int lineNumber) const = 0;
    virtual AsmList::Asm wvfs(Assembler::PlayDummyType dummyType,
                          AsmRegister reg, int length, int lineNumber) const = 0;
    virtual AsmList::Asm wvft(AsmRegister reg, int length, int lineNumber) const = 0;

    virtual AsmList::Asm brz(AsmRegister reg, const std::string& label,
                         bool noOpt, int lineNumber) const = 0;

    virtual AsmList::Asm ssl(AsmRegister reg, int lineNumber) const = 0;
    virtual AsmList::Asm ssr(AsmRegister reg, int lineNumber) const = 0;

    virtual AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const = 0;

    // Factory: selects Cervino or Hirzel based on device type.
    static std::unique_ptr<AsmCommandsImpl> getInstance(AwgDeviceType deviceType);
};

// Cervino implementation — older/FPGA devices.
//! \brief `AsmCommandsImpl` back end for the older Cervino / FPGA family.
//!
//! Encodes the subset of instructions supported by the Cervino
//! sequencer (UHFLI, UHFAWG, UHFQA, GHFLI, VHFLI), and throws for the
//! Hirzel-only forms (`wwvfq`, `wvfs`, `wvft`).  Reports
//! `isCWVFRSupported() == false`.
class AsmCommandsImplCervino : public AsmCommandsImpl {
public:
    ~AsmCommandsImplCervino() override;

    bool isCWVFRSupported() const override;  // returns false

    AsmList::Asm wwvfq(int lineNumber) const override;  // throws (unsupported)
    AsmList::Asm wprf(int lineNumber) const override;    // opcode 0xF0000000

    AsmList::Asm wvf(AsmRegister waveReg, AsmRegister dstReg,
                 int length, int lineNumber) const override;   // 0x20000000
    AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister dstReg,
                  int length, int lineNumber) const override;  // 0x30000000 or throws
    AsmList::Asm wvfs(Assembler::PlayDummyType, AsmRegister, int,
                  int) const override;   // throws
    AsmList::Asm wvft(AsmRegister, int, int) const override;  // throws

    AsmList::Asm brz(AsmRegister reg, const std::string& label,
                 bool noOpt, int lineNumber) const override;   // 0xF3000000

    AsmList::Asm ssl(AsmRegister reg, int lineNumber) const override;  // 0x60000005
    AsmList::Asm ssr(AsmRegister reg, int lineNumber) const override;  // 0x60000006

    AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const override;  // LD 0x60
};

// Hirzel implementation — HDAWG, UHF, SHF devices.
//! \brief `AsmCommandsImpl` back end for the Hirzel sequencer.
//!
//! Encodes the instruction set used by HDAWG, SHFQA, SHFSG, and SHFQC
//! devices, including the wavetable-quad (`wwvfq`) and waveform-end
//! (`wvfs`/`wvft`) forms unique to this family.  Reports
//! `isCWVFRSupported() == true`.
class AsmCommandsImplHirzel : public AsmCommandsImpl {
public:
    ~AsmCommandsImplHirzel() override;

    bool isCWVFRSupported() const override;  // returns true

    AsmList::Asm wwvfq(int lineNumber) const override;   // 0xF0000000
    AsmList::Asm wprf(int lineNumber) const override;     // INVALID (sentinel/no-op)

    AsmList::Asm wvf(AsmRegister waveReg, AsmRegister dstReg,
                 int length, int lineNumber) const override;
        // dstReg==R0: 0xFA000000; else: 0x20000000
    AsmList::Asm wvfi(AsmRegister, AsmRegister, int,
                  int) const override;  // always throws
    AsmList::Asm wvfs(Assembler::PlayDummyType dummyType,
                  AsmRegister reg, int length, int lineNumber) const override;
        // 0x30000001
    AsmList::Asm wvft(AsmRegister reg, int length, int lineNumber) const override;
        // 0xFC000000

    AsmList::Asm brz(AsmRegister reg, const std::string& label,
                 bool noOpt, int lineNumber) const override;
        // reg==R0: 0xFE000000; else: 0xF3000000

    AsmList::Asm ssl(AsmRegister reg, int lineNumber) const override;
        // 0x60000005, second slot = R0
    AsmList::Asm ssr(AsmRegister reg, int lineNumber) const override;
        // 0x60000006, second slot = R0

    AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const override;
        // LD 0x68
};

} // namespace zhinst
