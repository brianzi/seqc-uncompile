// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImpl — virtual base + Cervino/Hirzel declarations
// ============================================================================
#pragma once

#include <memory>
#include <string>

#include "asm_list.hpp"
#include "asm_register.hpp"
#include "assembler.hpp"
#include "types.hpp"

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
class AsmCommandsImpl {
public:
    virtual ~AsmCommandsImpl();

    virtual bool isCWVFRSupported() const = 0;

    virtual AsmList::Asm wwvfq(int lineNumber) const = 0;
    virtual AsmList::Asm wprf(int lineNumber) const = 0;

    virtual AsmList::Asm wvf(AsmRegister waveReg, AsmRegister markerReg,
                         int waveIndex, int lineNumber) const = 0;
    virtual AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister markerReg,
                          int waveIndex, int lineNumber) const = 0;
    virtual AsmList::Asm wvfs(Assembler::PlayDummyType dummyType,
                          AsmRegister reg, int arg, int lineNumber) const = 0;
    virtual AsmList::Asm wvft(AsmRegister reg, int arg, int lineNumber) const = 0;

    virtual AsmList::Asm brz(AsmRegister reg, const std::string& label,
                         bool flag, int lineNumber) const = 0;

    virtual AsmList::Asm ssl(AsmRegister reg, int lineNumber) const = 0;
    virtual AsmList::Asm ssr(AsmRegister reg, int lineNumber) const = 0;

    virtual AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const = 0;

    // Factory: selects Cervino or Hirzel based on device type.
    static std::unique_ptr<AsmCommandsImpl> getInstance(AwgDeviceType deviceType);
};

// Cervino implementation — older/FPGA devices.
class AsmCommandsImplCervino : public AsmCommandsImpl {
public:
    ~AsmCommandsImplCervino() override;

    bool isCWVFRSupported() const override;  // returns false

    AsmList::Asm wwvfq(int lineNumber) const override;  // throws (unsupported)
    AsmList::Asm wprf(int lineNumber) const override;    // opcode 0xF0000000

    AsmList::Asm wvf(AsmRegister waveReg, AsmRegister markerReg,
                 int waveIndex, int lineNumber) const override;   // 0x20000000
    AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister markerReg,
                  int waveIndex, int lineNumber) const override;  // 0x30000000 or throws
    AsmList::Asm wvfs(Assembler::PlayDummyType, AsmRegister, int,
                  int) const override;   // throws
    AsmList::Asm wvft(AsmRegister, int, int) const override;  // throws

    AsmList::Asm brz(AsmRegister reg, const std::string& label,
                 bool flag, int lineNumber) const override;   // 0xF3000000

    AsmList::Asm ssl(AsmRegister reg, int lineNumber) const override;  // 0x60000005
    AsmList::Asm ssr(AsmRegister reg, int lineNumber) const override;  // 0x60000006

    AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const override;  // LD 0x60
};

// Hirzel implementation — HDAWG, UHF, SHF devices.
class AsmCommandsImplHirzel : public AsmCommandsImpl {
public:
    ~AsmCommandsImplHirzel() override;

    bool isCWVFRSupported() const override;  // returns true

    AsmList::Asm wwvfq(int lineNumber) const override;   // 0xF0000000
    AsmList::Asm wprf(int lineNumber) const override;     // INVALID (sentinel/no-op)

    AsmList::Asm wvf(AsmRegister waveReg, AsmRegister markerReg,
                 int waveIndex, int lineNumber) const override;
        // markerReg==R0: 0xFA000000; else: 0x20000000
    AsmList::Asm wvfi(AsmRegister, AsmRegister, int,
                  int) const override;  // always throws
    AsmList::Asm wvfs(Assembler::PlayDummyType dummyType,
                  AsmRegister reg, int arg, int lineNumber) const override;
        // 0x30000001
    AsmList::Asm wvft(AsmRegister reg, int arg, int lineNumber) const override;
        // 0xFC000000

    AsmList::Asm brz(AsmRegister reg, const std::string& label,
                 bool flag, int lineNumber) const override;
        // reg==R0: 0xFE000000; else: 0xF3000000

    AsmList::Asm ssl(AsmRegister reg, int lineNumber) const override;
        // 0x60000005, second slot = R0
    AsmList::Asm ssr(AsmRegister reg, int lineNumber) const override;
        // 0x60000006, second slot = R0

    AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const override;
        // LD 0x68
};

} // namespace zhinst
